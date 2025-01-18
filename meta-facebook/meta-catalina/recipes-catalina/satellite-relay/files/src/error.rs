// SPDX-License-Identifier: Apache-2.0
//! Some boilerplate [1] for more ergonomic error handling in the axum request
//! handler.
//!
//! [1]: https://github.com/tokio-rs/axum/blob/main/examples/anyhow-error-response/src/main.rs

use axum::http::StatusCode;
use axum::response::IntoResponse;
use axum::response::Response;
use tracing::error;

pub struct RelayError(anyhow::Error);

impl IntoResponse for RelayError {
    fn into_response(self) -> Response {
        error!("{:?}", self.0.context("Relay error"));
        StatusCode::INTERNAL_SERVER_ERROR.into_response()
    }
}

impl<E> From<E> for RelayError
where
    E: Into<anyhow::Error>,
{
    fn from(e: E) -> Self {
        Self(e.into())
    }
}
