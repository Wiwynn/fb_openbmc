// SPDX-License-Identifier: Apache-2.0
//! There's a crate called `redfish-codegen` that provides bindings to Redfish
//! APIs, but it doesn't work that well. In practice, we need to access a very
//! small subset of the Redfish API, and validating the full schema of every
//! response increases the fragility of the application, because the Redfish
//! server might not actually be 100% schema-compliant.
//!
//! So, this module provides a minimal set of bindings to process Redfish APIs
//! this application needs to use.
//!
//! As an aside: the Redfish API has an incredibly large surface area, and
//! very weak library support. In practice, applications (server and client)
//! rarely use the official schemas to decode the JSON and instead just rely
//! on interacting with raw JSON objects (i.e., not even declaring the
//! bindings like below).

use anyhow::ensure;
use anyhow::Context;
use anyhow::Result;
use serde::Deserialize;
use serde::Serialize;
use std::collections::HashMap;
use tracing::warn;

#[derive(Clone, Default, Debug, Deserialize, Serialize, PartialEq)]
pub struct Id(String);

#[derive(Clone, Default, Debug, Deserialize, Serialize, PartialEq)]
pub struct IdRef {
    #[serde(rename = "@odata.id")]
    pub odata_id: Id,
}

#[derive(Clone, Default, Debug, Deserialize, Serialize, PartialEq)]
#[serde(rename_all = "PascalCase")]
pub struct ServiceRoot {
    pub event_service: IdRef,
}

#[derive(Clone, Default, Debug, Deserialize, Serialize, PartialEq)]
#[serde(rename_all = "PascalCase")]
pub struct EventService {
    pub subscriptions: IdRef,
}

#[derive(Clone, Default, Debug, Deserialize, Serialize, PartialEq)]
#[serde(rename_all = "PascalCase")]
pub struct EventDestinationCollection {
    pub members: Vec<IdRef>,
}

#[derive(Clone, Default, Debug, Deserialize, Serialize, PartialEq)]
#[serde(rename_all = "PascalCase")]
pub struct EventDestination {
    #[serde(rename = "@odata.id")]
    pub odata_id: Id,
    pub context: String,
    pub destination: String,
    pub event_format_type: EventFormatType,
    pub protocol: EventDestinationProtocol,
}

#[derive(Clone, Default, Debug, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "PascalCase")]
pub struct EventRecord {
    // FIXME: Message is not *supposed* to be optional, but HMC reset events do
    // not include it.
    pub message: Option<String>,
    pub message_id: Option<String>,
    pub message_severity: Option<Health>,
    pub severity: Option<Health>,
    // NOTE: Everything else just gets recorded as AdditionalData in when the
    // corresponding log is created.
    #[serde(flatten)]
    pub additional_data: HashMap<String, serde_json::Value>,
}

#[derive(Clone, Default, Debug, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "PascalCase")]
pub struct Event {
    // FIXME: `id` is supposed to always be a string, but the HMC uses an
    // integer in TaskService notifications.
    pub id: serde_json::Value,
    pub name: String,
    pub events: Vec<EventRecord>,
}

#[derive(Copy, Clone, Default, Debug, PartialEq, Deserialize, Serialize)]
pub enum EventFormatType {
    #[default]
    Event,
    MetricReport,
}

#[derive(Copy, Clone, Default, Debug, PartialEq, Deserialize, Serialize)]
pub enum Health {
    #[default]
    Invalid,
    OK,
    Warning,
    Critical,
    // FIXME: This is definitely *not* a valid variant of the Health enum used
    // in the MessageSeverity field of events, but some basic events from the
    // HMC (like, reset events) seem to accidentally use the phosphor-logging
    // level here. After reporting and fixing this in HMC firmware, we should
    // remove this variant.
    Informational,
}

#[derive(Copy, Clone, Default, Debug, PartialEq, Deserialize, Serialize)]
pub enum EventDestinationProtocol {
    #[default]
    Invalid,
    Redfish,
    Kafka,
    SNMPv1,
    SNMPv2c,
    SNMPv3,
    SMTP,
    SyslogTLS,
    SyslogTCP,
    SyslogUDP,
    SyslogRELP,
    OEM,
}

#[derive(Clone, Default, Debug, Deserialize, Serialize)]
pub struct RedfishError {
    pub error: RedfishErrorContents,
}

#[derive(Clone, Default, Debug, Deserialize, Serialize)]
pub struct RedfishErrorContents {
    // TODO: code and message are *not* optional, they are required, but the HMC
    // doesn't include them in most responses.
    pub code: Option<String>,
    pub message: Option<String>,
    #[serde(flatten)]
    pub extended_info: HashMap<String, Vec<Message>>,
}

#[derive(Clone, Default, Debug, Deserialize, Serialize)]
#[serde(rename_all = "PascalCase")]
pub struct Message {
    pub message: String,
    pub message_id: String,
    pub message_args: Vec<String>,
    pub message_severity: Health,
    pub resolution: String,
}

impl std::ops::Deref for Id {
    type Target = str;

    fn deref(&self) -> &str {
        &self.0
    }
}

/// The reqwest crate provides more flexibility than we need, this client
/// provides some simpler, less-flexible methods for interacting with a Redfish
/// endpoint.
pub struct Client {
    http: reqwest::Client,
    endpoint: reqwest::Url,
}

impl Client {
    pub fn new(endpoint: reqwest::Url) -> Result<Self> {
        let http = reqwest::Client::builder()
            .build()
            .context("Unable to create HTTP client")?;
        Ok(Self { http, endpoint })
    }

    pub async fn get<T>(&self, path: &str) -> Result<T>
    where
        T: serde::de::DeserializeOwned,
    {
        let url = self.endpoint.join(path)?;
        self.http
            .get(url.clone())
            .send()
            .await?
            .json::<T>()
            .await
            .with_context(|| format!("Unable to get {url}"))
    }

    pub async fn post<T>(&self, path: &str, body: &T) -> Result<()>
    where
        T: Serialize + ?Sized,
    {
        let url = self.endpoint.join(path)?;
        let text = self
            .http
            .post(url.clone())
            .json(body)
            .send()
            .await?
            .text()
            .await
            .with_context(|| format!("Unable to post to {url}"))?;

        let e = serde_json::from_str::<RedfishError>(&text)
            .or_else(|_e| -> Result<_> {
                let error = serde_json::from_str::<RedfishErrorContents>(&text)?;
                Ok(RedfishError { error })
            })
            .with_context(|| format!("Unable to decode response: {text}"))?;

        ensure!(
            e.error
                .extended_info
                .iter()
                .flat_map(|(_, ms)| ms.iter())
                .all(|m| m.message_severity == Health::OK),
            "Unable to setup event subscription on satellite controller: {e:#?}",
        );

        Ok(())
    }

    pub async fn delete(&self, path: &str) -> Result<()> {
        let url = self.endpoint.join(path)?;
        let text = self
            .http
            .delete(url.clone())
            .send()
            .await?
            .text()
            .await
            .with_context(|| format!("Unable to get {url}"))?;
        if !text.is_empty() {
            warn!("-X DELETE {url} returned text: {text}");
        }
        Ok(())
    }
}
