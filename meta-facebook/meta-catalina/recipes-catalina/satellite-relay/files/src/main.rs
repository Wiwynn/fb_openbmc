// SPDX-License-Identifier: Apache-2.0

use anyhow::Context;
use anyhow::Result;
use axum::extract::State;
use axum::routing::post;
use clap::Parser;
use reqwest::Url;
use satellite_relay::dbus;
use satellite_relay::error::RelayError;
use satellite_relay::redfish as rf;
use tokio::net::TcpListener;
use tokio::sync::oneshot;
use tokio::task::JoinSet;
use tracing::error;
use tracing::info;

use std::sync::Arc;
use std::time::Duration;

const SUBSCRIPTION_CONTEXT: &str = "satellite-relay";

/// Relay Redfish events from a satellite controller
///
/// Currently only supports relaying -X POST HTTP events. SSE may be supported
/// in the future.
#[derive(Debug, clap::Parser)]
struct Args {
    /// Satellite controller's Redfish endpoint
    #[arg(long, default_value = "http://172.31.13.251")]
    from: Url,

    /// Use this endpoint to receive events
    #[arg(long, default_value = "http://172.31.13.241:8888")]
    bind: Url,
}

async fn handle_event(
    State(dbus): State<Arc<zbus::Connection>>,
    body: String,
) -> Result<(), RelayError> {
    info!("{body}");

    let event = serde_json::from_str::<rf::Event>(&body)
        .with_context(|| format!("Error decoding Event: {body}"))?;

    let logging = dbus::LoggingCreateProxy::new(&dbus).await?;
    for record in &event.events {
        let dbus::SystemEventLog {
            message,
            severity,
            additional_data,
        } = record
            .try_into()
            .with_context(|| format!("Unable to convert Redfish event to SEL: {record:#?}"))?;
        let entry = logging
            .create(message, severity, additional_data)
            .await
            .context("Error creating log entry")?;
        info!("Created log entry {entry}");
    }

    Ok(())
}

/// Subscribe to events from a Redfish endpoint.
///
/// The Redfish endpoint should start sending events as POST requests to the
/// specified receiver-endpoint.
///
/// Note that the `to` URL must be reachable from the `from` endpoint.
async fn ensure_event_subscription(from: &Url, to: &Url, ctx: &str) -> Result<()> {
    let rf = rf::Client::new(from.clone())?;
    let rf = Arc::new(rf);
    let root = rf.get::<rf::ServiceRoot>("redfish/v1").await?;
    let event_service = rf
        .get::<rf::EventService>(&root.event_service.odata_id)
        .await?;
    // Get all the current event subscriptions
    let event_destinations = rf
        .get::<rf::EventDestinationCollection>(&event_service.subscriptions.odata_id)
        .await?
        .members
        .into_iter()
        .map(|id_ref| {
            let rf = rf.clone();
            async move { rf.get::<rf::EventDestination>(&id_ref.odata_id).await }
        })
        .collect::<JoinSet<Result<_>>>()
        .join_all()
        .await
        .into_iter()
        .collect::<Result<Vec<_>>>()
        .context("Unable to check current subscriptions")?;

    // Subscriptions associated with our "context" tag that do *not* match
    // the target configuration should be removed: these are usually old
    // subscriptions.
    let target = rf::EventDestination {
        odata_id: rf::Id::default(),
        context: ctx.into(),
        destination: to.as_str().into(),
        event_format_type: rf::EventFormatType::Event,
        protocol: rf::EventDestinationProtocol::Redfish,
    };
    let mut prev = None;
    let mut delete = Vec::new();
    for d in event_destinations {
        if d.context != target.context {
            // Ignore subscriptions created in other contexts
            continue;
        }

        // Ignore the ID field in equality comparison
        let mut target = target.clone();
        target.odata_id = d.odata_id.clone();

        // Delete subscriptions that don't match the target configuration.
        if d != target {
            delete.push(d);
            continue;
        }

        // This subscription matches the target configuration.
        // If we already found *another* subscription that matches
        // too, then this one is redundant, and should be deleted.
        if prev.is_some() {
            delete.push(d);
            continue;
        }

        prev = Some(d);
    }

    delete
        .into_iter()
        .map(|d| {
            let rf = rf.clone();
            async move { rf.delete(&d.odata_id).await }
        })
        .collect::<JoinSet<Result<_>>>()
        .join_all()
        .await
        .into_iter()
        .collect::<Result<Vec<_>>>()
        .context("Unable to delete old and/or redundant subscriptions")?;

    if let Some(prev) = prev {
        tracing::debug!("Existing subscription matches target configuration: {prev:#?}");
        return Ok(());
    }

    // Setup a new subscription, since no previous subscription matched.
    info!("Creating event subscription: {target:#?}");
    rf.post(&event_service.subscriptions.odata_id, &target)
        .await
}

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt::init();

    let args = Args::parse();
    let dbus = zbus::Connection::system()
        .await
        .context("Unable to connect to system dbus broker")?;
    let dbus = Arc::new(dbus);

    // Setup a TCP socket and an HTTP server to handle events sent by the
    // satellite.
    let addrs = args
        .bind
        .socket_addrs(|| None)
        .with_context(|| format!("Unable to resolve {}", args.bind))?;
    let listener = TcpListener::bind(&*addrs)
        .await
        .with_context(|| format!("Unable to bind to one of {addrs:?}"))?;
    let router = axum::Router::new()
        .route("/", post(handle_event))
        .with_state(dbus);
    let (tx, rx) = oneshot::channel::<()>();
    let server = axum::serve(listener, router)
        // If the subscriber loop fails, we want to stop the webserver and
        // exit. systemd will restart everything if the condition was an error.
        // Incorporating HMC_READY to gracefully detect HMC presence will
        // decrease noise.
        .with_graceful_shutdown(async move {
            if let Err(e) = rx.await {
                error!("shutdown channel error: {e:?}");
            }
        });
    let server = tokio::spawn(async move { server.await });

    // TODO: Monitor HMC_PRSNT_L and/or HMC_READY and check if we
    // need to resubscribe. The HMC *does* persist event subscriptions between
    // resets, but this is subject to change.
    let mut interval = tokio::time::interval(Duration::from_secs(20));
    let e = loop {
        interval.tick().await;

        if let Err(e) =
            ensure_event_subscription(&args.from, &args.bind, SUBSCRIPTION_CONTEXT).await
        {
            break e;
        }
    };

    // Try to shutdown the http server gracefully
    if let Err(e) = tx.send(()) {
        error!("Listening task closed shutdown channel: {e:?}");
    }
    if let Err(e) = server.await {
        error!("Server shutdown error: {e:?}");
    }

    Err(e).with_context(|| format!("Unable to ensure subscription to events from {}", args.from))
}
