// SPDX-License-Identifier: Apache-2.0

use anyhow::Context;
use serde::Deserialize;
use serde::Serialize;
use zbus::zvariant::OwnedObjectPath;

use std::collections::HashMap;

use crate::redfish as rf;

/// `z-bus` bindings to the object mapper, for scanning entity-manager inventory
/// for satellite controllers at startup.
///
/// Example of finding satellite sensors in entity-manager inventory:
///
/// ```rust,no_run
/// use satellite_relay::dbus::ObjectMapperProxy;
/// use satellite_relay::dbus::SatelliteControllerConfigurationProxy;
///
/// #[tokio::main]
/// async fn main() -> anyhow::Result<()> {
///     let dbus = zbus::Connection::system().await?;
///     let mapper = ObjectMapperProxy::new(&dbus).await?;
///     let paths = mapper
///         .get_sub_tree_paths(
///             "/xyz/openbmc_project/inventory".into(),
///             0,
///             vec!["xyz.openbmc_project.Configuration.SatelliteController".into()],
///         )
///         .await?;
///
///     assert_eq!(paths.len(), 1, "There should be exactly one satellite");
///     let path = paths[0].as_str();
///     println!("Found 1 satellite controller:\n - {path}");
///
///     let config = SatelliteControllerConfigurationProxy::builder(&dbus)
///         .path(path)?
///         .build()
///         .await?;
///     let host = config.hostname().await?;
///     let port = config.port().await?;
///     println!("Satellite location:\n - {host}:{port}");
///
///     Ok(())
/// }
/// ```
#[zbus::proxy(
    default_service = "xyz.openbmc_project.ObjectMapper",
    default_path = "/xyz/openbmc_project/object_mapper",
    interface = "xyz.openbmc_project.ObjectMapper",
    gen_blocking = false
)]
pub trait ObjectMapper {
    async fn get_sub_tree_paths(
        &self,
        subtree: String,
        depth: i32,
        interfaces: Vec<String>,
    ) -> zbus::Result<Vec<String>>;
}

/// `z-bus` binding for satellite controller configuration objects provided
/// by entity-manager.
///
/// For NVIDIA SKUs, this provides the IP address and port number of the HMC
/// Redfish API.
///
/// Example handling satellite controller inventory being added:
///
/// ```rust,no_run
/// use futures_util::stream::StreamExt;
///
/// #[tokio::main]
/// async fn main() -> anyhow::Result<()> {
///     let dbus = zbus::Connection::system().await?;
///     let inventory = zbus::fdo::ObjectManagerProxy::new(
///         &dbus,
///         "xyz.openbmc_project.EntityManager",
///         "/xyz/openbmc_project/inventory",
///     ).await?;
///
///     // Handle interfaces being added to the inventory path.
///     let mut inventory_added = inventory.receive_interfaces_added().await?;
///     while let Some(signal) = inventory_added.next().await {
///         let args = signal.args()?;
///         let v = args.interfaces_and_properties
///             .get("xyz.openbmc_project.Configuration.SatelliteController");
///         let Some(v) = v else {
///             continue;
///         };
///         println!("A new satellite was added: {v:?}");
///     }
///
///     Ok(())
/// }
/// ```
#[zbus::proxy(
    default_service = "xyz.openbmc_project.EntityManager",
    interface = "xyz.openbmc_project.Configuration.SatelliteController",
    gen_blocking = false
)]
pub trait SatelliteControllerConfiguration {
    #[zbus(property)]
    fn auth_type(&self) -> zbus::Result<String>;

    #[zbus(property)]
    fn hostname(&self) -> zbus::Result<String>;

    #[zbus(property)]
    fn name(&self) -> zbus::Result<String>;

    #[zbus(property)]
    fn port(&self) -> zbus::Result<u64>;

    #[zbus(property, name = "Type")]
    fn type_(&self) -> zbus::Result<String>;
}

#[derive(Debug, Copy, Clone, Serialize, Deserialize, zbus::zvariant::Type, PartialEq)]
#[zvariant(signature = "s")]
pub enum Level {
    #[serde(rename = "xyz.openbmc_project.Logging.Entry.Level.Emergency")]
    Emergency,
    #[serde(rename = "xyz.openbmc_project.Logging.Entry.Level.Alert")]
    Alert,
    #[serde(rename = "xyz.openbmc_project.Logging.Entry.Level.Critical")]
    Critical,
    #[serde(rename = "xyz.openbmc_project.Logging.Entry.Level.Error")]
    Error,
    #[serde(rename = "xyz.openbmc_project.Logging.Entry.Level.Warning")]
    Warning,
    #[serde(rename = "xyz.openbmc_project.Logging.Entry.Level.Notice")]
    Notice,
    #[serde(rename = "xyz.openbmc_project.Logging.Entry.Level.Informational")]
    Informational,
    #[serde(rename = "xyz.openbmc_project.Logging.Entry.Level.Debug")]
    Debug,
}

#[zbus::proxy(
    default_service = "xyz.openbmc_project.Logging",
    default_path = "/xyz/openbmc_project/logging",
    interface = "xyz.openbmc_project.Logging.Create",
    gen_blocking = false
)]
pub trait LoggingCreate {
    async fn create(
        &self,
        message: String,
        severity: Level,
        additional_data: HashMap<String, String>,
    ) -> zbus::Result<OwnedObjectPath>;
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct SystemEventLog {
    pub message: String,
    pub severity: Level,
    pub additional_data: HashMap<String, String>,
}

impl<'a> TryFrom<&'a rf::EventRecord> for SystemEventLog {
    type Error = anyhow::Error;

    fn try_from(record: &'a rf::EventRecord) -> Result<Self, Self::Error> {
        let message = record
            .message
            .as_deref()
            .unwrap_or("An event without a Message occurred")
            .to_string();
        let message_id = record.message_id.as_deref().unwrap_or_default().to_string();
        let severity = record
            .message_severity
            .or(record.severity)
            .unwrap_or_default();

        let mut additional_data = HashMap::with_capacity(record.additional_data.len());

        // TODO(T212903929): Fields that we extracted should generally be
        // included in the additional data too, but we should really consider
        // just accessing HMC event logs directly through Redfish aggregation
        // (proxying) so that we don't have to implement any transformation
        // at all.
        additional_data.insert("Message".into(), message.clone());
        additional_data.insert("MessageId".into(), message_id);
        additional_data.insert("MessageSeverity".into(), format!("{severity:?}"));

        for (k, v) in &record.additional_data {
            let k = k.clone();
            // Avoid adding escaped double-quotes to something that's already
            // a string.
            if let serde_json::Value::String(v) = v {
                additional_data.insert(k, v.clone());
                continue;
            }

            let v =
                serde_json::to_string(&v).with_context(|| format!("Unable to serialize {v:#?}"))?;
            additional_data.insert(k, v);
        }

        let severity = match severity {
            rf::Health::Invalid => Level::Error,
            rf::Health::OK | rf::Health::Informational => Level::Informational,
            rf::Health::Warning => Level::Warning,
            rf::Health::Critical => Level::Critical,
        };

        Ok(Self {
            message,
            severity,
            additional_data,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;
    use zbus::zvariant::serialized::Context;
    use zbus::zvariant::to_bytes;
    use zbus::zvariant::LE;

    #[test]
    fn test_enum_repr() {
        let cx = Context::new_dbus(LE, 0);
        let encoded = to_bytes(cx, &Level::Emergency).unwrap();
        let decoded: &str = encoded.deserialize().unwrap().0;
        assert_eq!(decoded, "xyz.openbmc_project.Logging.Entry.Level.Emergency");
    }

    #[test]
    fn test_storage_warning_to_sel() {
        let event = json!({
          "EventTimestamp": "2025-01-16T06:15:37+00:00",
          "EventType": "Event",
          "LogEntry": {
            "@odata.id": "/redfish/v1/Systems/HGX_Baseboard_0/LogServices/EventLog/Entries/23155"
          },
          "Message": "Processes consuming HIGH Resource Storage are ",
          "MessageArgs": [
            "Storage",
            ""
          ],
          "MessageId": "OpenBMC.0.4.BMCSystemResourceInfo",
          "MessageSeverity": "Warning",
          "Oem": {
            "Nvidia": {
              "@odata.type": "#NvidiaEvent.v1_0_0.EventRecord",
              "Device": "",
              "ErrorId": ""
            }
          },
          "Resolution": "None"
        });
        let sel = json!({
          "message": "Processes consuming HIGH Resource Storage are ",
          "severity": "xyz.openbmc_project.Logging.Entry.Level.Warning",
          "additional_data": {
            "Message": "Processes consuming HIGH Resource Storage are ",
            "MessageArgs": "[\"Storage\",\"\"]",
            "MessageId": "OpenBMC.0.4.BMCSystemResourceInfo",
            "MessageSeverity": "Warning",
            "Oem": "{\"Nvidia\":{\"@odata.type\":\"#NvidiaEvent.v1_0_0.EventRecord\",\"Device\":\"\",\"ErrorId\":\"\"}}",
            "EventTimestamp": "2025-01-16T06:15:37+00:00",
            "EventType": "Event",
            "Resolution": "None",
            "LogEntry": "{\"@odata.id\":\"/redfish/v1/Systems/HGX_Baseboard_0/LogServices/EventLog/Entries/23155\"}"
          },
        });
        test_rf_to_sel(event, sel);
    }

    #[test]
    fn test_cper_to_sel() {
        let event = json!({
          "CPER": {
            "NotificationType": "09a9d5ac-5204-4214-96e5-94992e752bcd",
            "Oem": {
                "Nvidia": {
                  "@odata.type": "#NvidiaCPER.v1_0_0.NvidiaCPER",
                  "Pcie": {
                    "AerInfo": {
                      "Capabilites_control": 172,
                      "Capability_header": 318832641,
                      "Correctable_error_mask": 24576,
                      "Correctable_error_status": 8192,
                      "Correctable_error_status_hex": "0x00002000",
                      "Data": "AQABEwAQEAAAAFAAECBGAAAgAAAAYAAArAAAAARAAEoQAAAAAAEACP////8AAAAAAgABGgAIAAAAAAAAAAAAAAEAPwX/AACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
                      "Tlp_header_0": 1241530372,
                      "Tlp_header_1": 16,
                      "Tlp_header_2": 134217984,
                      "Tlp_header_3": 4294967295_u32,
                      "Uncorrectable_error_mask": 5242880,
                      "Uncorrectable_error_severity": 4595728,
                      "Uncorrectable_error_status": 1052672,
                      "Uncorrectable_error_status_hex": "0x00101000"
                    },
                    "BridgeControlStatus": {
                      "ControlRegister": 18,
                      "SecondaryStatusRegister": 0
                    },
                    "CapabilityStructure": {
                      "Data": "EKRSAAKAEAAHAAAAEzBEAEAAExAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAOAAAAAwAeAAAAAAAAAAAA"
                    },
                    "CommandStatus": {
                      "CommandRegister": 263,
                      "StatusRegister": 32784
                    },
                    "DeviceID": {
                      "ClassCode": 1030,
                      "DeviceID": 49160,
                      "DeviceIDHex": "0xC008",
                      "DeviceNumber": 0,
                      "FunctionNumber": 0,
                      "PrimaryOrDeviceBusNumber": 1,
                      "SecondaryBusNumber": 0,
                      "SegmentNumber": 5,
                      "SlotNumber": 0,
                      "VendorID": 4824
                    },
                    "DeviceSerialNumber": 0,
                    "PortType": {
                      "Name": "Upstream Switch Port",
                      "Value": 5
                    },
                    "ValidationBits": {
                      "AerInfoValid": true,
                      "BridgeControlStatusValid": true,
                      "CapabilityStructureStatusValid": true,
                      "CommandStatusValid": true,
                      "DeviceIDValid": true,
                      "DeviceSerialNumberValid": false,
                      "PortTypeValid": true,
                      "VersionValid": false
                    },
                    "Version": {
                      "Major": 0,
                      "Minor": 0
                    }
                  }
                }
              },
              "SectionType": "d995e954-bbc1-430f-ad91-b44dcb3c6f35"
            },
            "Created": "2025-01-16T19:04:48+00:00",
            "DiagnosticData": "Q1BFUgEB/////wEAAgAAAAIAAACYAQAASAQZABYBJSAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAHjkAalzEe8Rlqhfpr717qSs1akJBFIUQpbllJkudSvNAwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADIAAAA0AAAAAAAAQAAAAAAVOmV2cG7D0OtkbRNyzxvNUozT8xjxesRj4ifesdsbwwCAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADtAAAAAAAAAAUAAAAAAAAABwEQgAAAAAD
        \n          YEgjAAAQGAAAFAAEAAAAAAAAAAAAAAAAAABIAEKRSAAKAEAAHAAAAEzBEAEAAExAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAOAAAAAwAeAAAAAAAAAAAAAQABEwAQEAAAAFAAECBGAAAgAAAAYAAArAAAAARAAEoQAAAAAAEACP////8AAAAAAgABGgAIAAAAAAAAAAAAAAEAPwX/AACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
            "DiagnosticDataType": "CPERSection",
            "EventTimestamp": "2025-01-16T19:04:46+00:00",
            "EventType": "Event",
            "LogEntry": {
              "@odata.id": "/redfish/v1/Systems/HGX_Baseboard_0/LogServices/EventLog/Entries/48973"
            },
            "Message": "A platform error occurred.",
            "MessageId": "Platform.1.0.PlatformError",
            "MessageSeverity": "OK",
            "Oem": {
              "Nvidia": {
                "@odata.type": "#NvidiaEvent.v1_0_0.EventRecord",
                "Device": "",
                "ErrorId": ""
              }
            },
          "Severity": "OK"
        });
        let expected = json!({
          "additional_data": {
            "CPER": "{\"NotificationType\":\"09a9d5ac-5204-4214-96e5-94992e752bcd\",\"Oem\":{\"Nvidia\":{\"@odata.type\":\"#NvidiaCPER.v1_0_0.NvidiaCPER\",\"Pcie\":{\"AerInfo\":{\"Capabilites_control\":172,\"Capability_header\":318832641,\"Correctable_error_mask\":24576,\"Correctable_error_status\":8192,\"Correctable_error_status_hex\":\"0x00002000\",\"Data\":\"AQABEwAQEAAAAFAAECBGAAAgAAAAYAAArAAAAARAAEoQAAAAAAEACP////8AAAAAAgABGgAIAAAAAAAAAAAAAAEAPwX/AACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\",\"Tlp_header_0\":1241530372,\"Tlp_header_1\":16,\"Tlp_header_2\":134217984,\"Tlp_header_3\":4294967295,\"Uncorrectable_error_mask\":5242880,\"Uncorrectable_error_severity\":4595728,\"Uncorrectable_error_status\":1052672,\"Uncorrectable_error_status_hex\":\"0x00101000\"},\"BridgeControlStatus\":{\"ControlRegister\":18,\"SecondaryStatusRegister\":0},\"CapabilityStructure\":{\"Data\":\"EKRSAAKAEAAHAAAAEzBEAEAAExAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAOAAAAAwAeAAAAAAAAAAAA\"},\"CommandStatus\":{\"CommandRegister\":263,\"StatusRegister\":32784},\"DeviceID\":{\"ClassCode\":1030,\"DeviceID\":49160,\"DeviceIDHex\":\"0xC008\",\"DeviceNumber\":0,\"FunctionNumber\":0,\"PrimaryOrDeviceBusNumber\":1,\"SecondaryBusNumber\":0,\"SegmentNumber\":5,\"SlotNumber\":0,\"VendorID\":4824},\"DeviceSerialNumber\":0,\"PortType\":{\"Name\":\"Upstream Switch Port\",\"Value\":5},\"ValidationBits\":{\"AerInfoValid\":true,\"BridgeControlStatusValid\":true,\"CapabilityStructureStatusValid\":true,\"CommandStatusValid\":true,\"DeviceIDValid\":true,\"DeviceSerialNumberValid\":false,\"PortTypeValid\":true,\"VersionValid\":false},\"Version\":{\"Major\":0,\"Minor\":0}}}},\"SectionType\":\"d995e954-bbc1-430f-ad91-b44dcb3c6f35\"}",
            "Created": "2025-01-16T19:04:48+00:00",
            "DiagnosticData": "Q1BFUgEB/////wEAAgAAAAIAAACYAQAASAQZABYBJSAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAHjkAalzEe8Rlqhfpr717qSs1akJBFIUQpbllJkudSvNAwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADIAAAA0AAAAAAAAQAAAAAAVOmV2cG7D0OtkbRNyzxvNUozT8xjxesRj4ifesdsbwwCAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADtAAAAAAAAAAUAAAAAAAAABwEQgAAAAAD\n        \n          YEgjAAAQGAAAFAAEAAAAAAAAAAAAAAAAAABIAEKRSAAKAEAAHAAAAEzBEAEAAExAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAOAAAAAwAeAAAAAAAAAAAAAQABEwAQEAAAAFAAECBGAAAgAAAAYAAArAAAAARAAEoQAAAAAAEACP////8AAAAAAgABGgAIAAAAAAAAAAAAAAEAPwX/AACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
            "DiagnosticDataType": "CPERSection",
            "EventTimestamp": "2025-01-16T19:04:46+00:00",
            "EventType": "Event",
            "LogEntry": "{\"@odata.id\":\"/redfish/v1/Systems/HGX_Baseboard_0/LogServices/EventLog/Entries/48973\"}",
            "Message": "A platform error occurred.",
            "MessageId": "Platform.1.0.PlatformError",
            "MessageSeverity": "OK",
            "Oem": "{\"Nvidia\":{\"@odata.type\":\"#NvidiaEvent.v1_0_0.EventRecord\",\"Device\":\"\",\"ErrorId\":\"\"}}"
          },
          "message": "A platform error occurred.",
          "severity": "xyz.openbmc_project.Logging.Entry.Level.Informational"
        });
        test_rf_to_sel(event, expected);
    }

    fn test_rf_to_sel(event: serde_json::Value, expected: serde_json::Value) {
        println!("{event:#}");
        let event: rf::EventRecord = serde_json::from_value(event).unwrap();
        let event = &event;
        let sel: SystemEventLog = event.try_into().unwrap();
        let sel = serde_json::to_value(sel).unwrap();
        assert_eq!(sel, expected, "Expected {expected:#}, got {sel:#}");
        println!("{sel:#}");
    }
}
