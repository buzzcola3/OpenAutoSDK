#include "buzz/autoapp/ProtoTrace.hpp"

#include <Common/Data.hpp>
#include <Messenger/ChannelId.hpp>
#include <Messenger/Message.hpp>
#include <Messenger/MessageId.hpp>

#include <aap_protobuf/service/bluetooth/BluetoothMessageId.pb.h>
#include <aap_protobuf/service/bluetooth/message/BluetoothAuthenticationData.pb.h>
#include <aap_protobuf/service/bluetooth/message/BluetoothAuthenticationResult.pb.h>
#include <aap_protobuf/service/bluetooth/message/BluetoothPairingRequest.pb.h>
#include <aap_protobuf/service/bluetooth/message/BluetoothPairingResponse.pb.h>
#include <aap_protobuf/service/control/ControlMessageType.pb.h>
#include <aap_protobuf/service/control/message/AuthResponse.pb.h>
#include <aap_protobuf/service/control/message/AudioFocusNotification.pb.h>
#include <aap_protobuf/service/control/message/AudioFocusRequest.pb.h>
#include <aap_protobuf/service/control/message/BatteryStatusNotification.pb.h>
#include <aap_protobuf/service/control/message/ByeByeRequest.pb.h>
#include <aap_protobuf/service/control/message/ByeByeResponse.pb.h>
#include <aap_protobuf/service/control/message/ChannelOpenRequest.pb.h>
#include <aap_protobuf/service/control/message/ChannelOpenResponse.pb.h>
#include <aap_protobuf/service/control/message/NavFocusNotification.pb.h>
#include <aap_protobuf/service/control/message/NavFocusRequestNotification.pb.h>
#include <aap_protobuf/service/control/message/PingRequest.pb.h>
#include <aap_protobuf/service/control/message/PingResponse.pb.h>
#include <aap_protobuf/service/control/message/ServiceDiscoveryRequest.pb.h>
#include <aap_protobuf/service/control/message/ServiceDiscoveryResponse.pb.h>
#include <aap_protobuf/service/control/message/VoiceSessionNotification.pb.h>
#include <aap_protobuf/service/inputsource/InputMessageId.pb.h>
#include <aap_protobuf/service/inputsource/message/InputReport.pb.h>
#include <aap_protobuf/service/media/shared/message/Config.pb.h>
#include <aap_protobuf/service/media/shared/message/Setup.pb.h>
#include <aap_protobuf/service/media/shared/message/Start.pb.h>
#include <aap_protobuf/service/media/shared/message/Stop.pb.h>
#include <aap_protobuf/service/media/sink/MediaMessageId.pb.h>
#include <aap_protobuf/service/media/sink/message/KeyBindingRequest.pb.h>
#include <aap_protobuf/service/media/sink/message/KeyBindingResponse.pb.h>
#include <aap_protobuf/service/media/source/message/Ack.pb.h>
#include <aap_protobuf/service/media/source/message/MicrophoneRequest.pb.h>
#include <aap_protobuf/service/media/source/message/MicrophoneResponse.pb.h>
#include <aap_protobuf/service/media/video/message/VideoFocusNotification.pb.h>
#include <aap_protobuf/service/media/video/message/VideoFocusRequestNotification.pb.h>
#include <aap_protobuf/service/mediaplayback/MediaPlaybackStatusMessageId.pb.h>
#include <aap_protobuf/service/mediaplayback/message/MediaPlaybackMetadata.pb.h>
#include <aap_protobuf/service/mediaplayback/message/MediaPlaybackStatus.pb.h>
#include <aap_protobuf/service/navigationstatus/NavigationStatusMessageId.pb.h>
#include <aap_protobuf/service/navigationstatus/message/NavigationNextTurnDistanceEvent.pb.h>
#include <aap_protobuf/service/navigationstatus/message/NavigationNextTurnEvent.pb.h>
#include <aap_protobuf/service/navigationstatus/message/NavigationStatus.pb.h>
#include <aap_protobuf/service/sensorsource/SensorMessageId.pb.h>
#include <aap_protobuf/service/sensorsource/message/SensorBatch.pb.h>
#include <aap_protobuf/service/sensorsource/message/SensorRequest.pb.h>
#include <aap_protobuf/service/sensorsource/message/SensorStartResponseMessage.pb.h>
#include <aap_protobuf/service/wifiprojection/WifiProjectionMessageId.pb.h>
#include <aap_protobuf/service/wifiprojection/message/WifiCredentialsRequest.pb.h>
#include <aap_protobuf/service/wifiprojection/message/WifiCredentialsResponse.pb.h>

#include <string>

#include "buzz/autoapp/ProtoDebugRecorder.hpp"

namespace buzz::autoapp::debug {
namespace {

using ChannelId = aasdk::messenger::ChannelId;
using aasdk::messenger::channelIdToString;
using DataConstBuffer = aasdk::common::DataConstBuffer;
using ControlMessageType = aap_protobuf::service::control::message::ControlMessageType;
using SensorMessageId = aap_protobuf::service::sensorsource::SensorMessageId;
using MediaMessageId = aap_protobuf::service::media::sink::MediaMessageId;
using InputMessageId = aap_protobuf::service::inputsource::InputMessageId;
using BluetoothMessageId = aap_protobuf::service::bluetooth::BluetoothMessageId;
using WifiProjectionMessageId = aap_protobuf::service::wifiprojection::WifiProjectionMessageId;
using NavigationStatusMessageId = aap_protobuf::service::navigationstatus::NavigationStatusMessageId;
using MediaPlaybackStatusMessageId = aap_protobuf::service::mediaplayback::MediaPlaybackStatusMessageId;

std::string BuildLabel(ProtoTraceDirection direction,
                       const std::string& channel,
                       const char* message) {
  std::string label = channel;
  label.push_back('.');
  label.append(message);
  label.append(direction == ProtoTraceDirection::kTx ? " (TX)" : " (RX)");
  return label;
}

template <typename Proto>
bool DecodeAndRecord(ProtoTraceDirection direction,
                     const std::string& channel,
                     const char* message,
                     const DataConstBuffer& payload) {
  Proto proto;
  if (!proto.ParseFromArray(payload.cdata, payload.size)) {
    return false;
  }

  RecordProtoMessage(channel, BuildLabel(direction, channel, message), proto);
  return true;
}

std::string ChannelLabel(ChannelId channel) {
  switch (channel) {
    case ChannelId::CONTROL:
      return "Control";
    case ChannelId::SENSOR:
      return "Sensor";
    case ChannelId::MEDIA_SINK:
      return "MediaSink";
    case ChannelId::MEDIA_SINK_VIDEO:
      return "VideoMediaSink";
    case ChannelId::MEDIA_SINK_MEDIA_AUDIO:
      return "MediaAudioSink";
    case ChannelId::MEDIA_SINK_GUIDANCE_AUDIO:
      return "GuidanceAudioSink";
    case ChannelId::MEDIA_SINK_SYSTEM_AUDIO:
      return "SystemAudioSink";
    case ChannelId::MEDIA_SINK_TELEPHONY_AUDIO:
      return "TelephonyAudioSink";
    case ChannelId::MEDIA_SOURCE_MICROPHONE:
      return "MediaSourceMic";
    case ChannelId::INPUT_SOURCE:
      return "InputSource";
    case ChannelId::BLUETOOTH:
      return "Bluetooth";
    case ChannelId::NAVIGATION_STATUS:
      return "NavigationStatus";
    case ChannelId::MEDIA_PLAYBACK_STATUS:
      return "MediaPlaybackStatus";
    case ChannelId::MEDIA_BROWSER:
      return "MediaBrowser";
    case ChannelId::PHONE_STATUS:
      return "PhoneStatus";
    case ChannelId::RADIO:
      return "Radio";
    case ChannelId::GENERIC_NOTIFICATION:
      return "GenericNotification";
    case ChannelId::VENDOR_EXTENSION:
      return "VendorExtension";
    case ChannelId::WIFI_PROJECTION:
      return "WifiProjection";
    default:
      return channelIdToString(channel);
  }
}

bool TraceChannelOpenMessages(ProtoTraceDirection direction,
                              int message_id,
                              const std::string& channel,
                              const DataConstBuffer& payload) {
  switch (message_id) {
    case ControlMessageType::MESSAGE_CHANNEL_OPEN_REQUEST:
      return DecodeAndRecord<aap_protobuf::service::control::message::ChannelOpenRequest>(
          direction, channel, "ChannelOpenRequest", payload);
    case ControlMessageType::MESSAGE_CHANNEL_OPEN_RESPONSE:
      return DecodeAndRecord<aap_protobuf::service::control::message::ChannelOpenResponse>(
          direction, channel, "ChannelOpenResponse", payload);
    default:
      return false;
  }
}

bool TraceControlChannel(ProtoTraceDirection direction,
                         int message_id,
                         const std::string& channel,
                         const DataConstBuffer& payload) {
  switch (message_id) {
    case ControlMessageType::MESSAGE_SERVICE_DISCOVERY_REQUEST:
      return DecodeAndRecord<aap_protobuf::service::control::message::ServiceDiscoveryRequest>(
          direction, channel, "ServiceDiscoveryRequest", payload);
    case ControlMessageType::MESSAGE_SERVICE_DISCOVERY_RESPONSE:
      return DecodeAndRecord<aap_protobuf::service::control::message::ServiceDiscoveryResponse>(
          direction, channel, "ServiceDiscoveryResponse", payload);
    case ControlMessageType::MESSAGE_AUDIO_FOCUS_REQUEST:
      return DecodeAndRecord<aap_protobuf::service::control::message::AudioFocusRequest>(
          direction, channel, "AudioFocusRequest", payload);
    case ControlMessageType::MESSAGE_AUDIO_FOCUS_NOTIFICATION:
      return DecodeAndRecord<aap_protobuf::service::control::message::AudioFocusNotification>(
          direction, channel, "AudioFocusResponse", payload);
    case ControlMessageType::MESSAGE_NAV_FOCUS_REQUEST:
      return DecodeAndRecord<aap_protobuf::service::control::message::NavFocusRequestNotification>(
          direction, channel, "NavigationFocusRequest", payload);
    case ControlMessageType::MESSAGE_NAV_FOCUS_NOTIFICATION:
      return DecodeAndRecord<aap_protobuf::service::control::message::NavFocusNotification>(
          direction, channel, "NavigationFocusResponse", payload);
    case ControlMessageType::MESSAGE_VOICE_SESSION_NOTIFICATION:
      return DecodeAndRecord<aap_protobuf::service::control::message::VoiceSessionNotification>(
          direction, channel, "VoiceSession", payload);
    case ControlMessageType::MESSAGE_PING_REQUEST:
      return DecodeAndRecord<aap_protobuf::service::control::message::PingRequest>(
          direction, channel, "PingRequest", payload);
    case ControlMessageType::MESSAGE_PING_RESPONSE:
      return DecodeAndRecord<aap_protobuf::service::control::message::PingResponse>(
          direction, channel, "PingResponse", payload);
    case ControlMessageType::MESSAGE_BYEBYE_REQUEST:
      return DecodeAndRecord<aap_protobuf::service::control::message::ByeByeRequest>(
          direction, channel, "ByeByeRequest", payload);
    case ControlMessageType::MESSAGE_BYEBYE_RESPONSE:
      return DecodeAndRecord<aap_protobuf::service::control::message::ByeByeResponse>(
          direction, channel, "ByeByeResponse", payload);
    case ControlMessageType::MESSAGE_BATTERY_STATUS_NOTIFICATION:
      return DecodeAndRecord<aap_protobuf::service::control::message::BatteryStatusNotification>(
          direction, channel, "BatteryStatusNotification", payload);
    case ControlMessageType::MESSAGE_AUTH_COMPLETE:
      return DecodeAndRecord<aap_protobuf::service::control::message::AuthResponse>(
          direction, channel, "AuthComplete", payload);
    default:
      return TraceChannelOpenMessages(direction, message_id, channel, payload);
  }
}

bool TraceSensorChannel(ProtoTraceDirection direction,
                        int message_id,
                        const std::string& channel,
                        const DataConstBuffer& payload) {
  if (TraceChannelOpenMessages(direction, message_id, channel, payload)) {
    return true;
  }

  switch (message_id) {
    case SensorMessageId::SENSOR_MESSAGE_REQUEST:
      return DecodeAndRecord<aap_protobuf::service::sensorsource::message::SensorRequest>(
          direction, channel, "SensorStartRequest", payload);
    case SensorMessageId::SENSOR_MESSAGE_RESPONSE:
      return DecodeAndRecord<aap_protobuf::service::sensorsource::message::SensorStartResponseMessage>(
          direction, channel, "SensorStartResponse", payload);
    case SensorMessageId::SENSOR_MESSAGE_BATCH:
      return DecodeAndRecord<aap_protobuf::service::sensorsource::message::SensorBatch>(
          direction, channel, "SensorBatch", payload);
    default:
      return false;
  }
}

bool TraceMediaSinkChannel(ProtoTraceDirection direction,
                           int message_id,
                           const std::string& channel,
                           const DataConstBuffer& payload) {
  if (TraceChannelOpenMessages(direction, message_id, channel, payload)) {
    return true;
  }

  switch (message_id) {
    case MediaMessageId::MEDIA_MESSAGE_SETUP:
      return DecodeAndRecord<aap_protobuf::service::media::shared::message::Setup>(
          direction, channel, "Setup", payload);
    case MediaMessageId::MEDIA_MESSAGE_CONFIG:
      return DecodeAndRecord<aap_protobuf::service::media::shared::message::Config>(
          direction, channel, "Config", payload);
    case MediaMessageId::MEDIA_MESSAGE_START:
      return DecodeAndRecord<aap_protobuf::service::media::shared::message::Start>(
          direction, channel, "Start", payload);
    case MediaMessageId::MEDIA_MESSAGE_STOP:
      return DecodeAndRecord<aap_protobuf::service::media::shared::message::Stop>(
          direction, channel, "Stop", payload);
    case MediaMessageId::MEDIA_MESSAGE_ACK:
      return DecodeAndRecord<aap_protobuf::service::media::source::message::Ack>(
          direction, channel, "Ack", payload);
    case MediaMessageId::MEDIA_MESSAGE_VIDEO_FOCUS_REQUEST:
      return DecodeAndRecord<aap_protobuf::service::media::video::message::VideoFocusRequestNotification>(
          direction, channel, "VideoFocusRequest", payload);
    case MediaMessageId::MEDIA_MESSAGE_VIDEO_FOCUS_NOTIFICATION:
      return DecodeAndRecord<aap_protobuf::service::media::video::message::VideoFocusNotification>(
          direction, channel, "VideoFocusNotification", payload);
    default:
      return false;
  }
}

bool TraceMediaSourceChannel(ProtoTraceDirection direction,
                             int message_id,
                             const std::string& channel,
                             const DataConstBuffer& payload) {
  if (TraceChannelOpenMessages(direction, message_id, channel, payload)) {
    return true;
  }

  switch (message_id) {
    case MediaMessageId::MEDIA_MESSAGE_SETUP:
      if (direction == ProtoTraceDirection::kRx) {
        return DecodeAndRecord<aap_protobuf::service::media::shared::message::Setup>(
            direction, channel, "Setup", payload);
      }
      return DecodeAndRecord<aap_protobuf::service::media::shared::message::Config>(
          direction, channel, "Config", payload);
    case MediaMessageId::MEDIA_MESSAGE_MICROPHONE_REQUEST:
      if (direction == ProtoTraceDirection::kRx) {
        return DecodeAndRecord<aap_protobuf::service::media::source::message::MicrophoneRequest>(
            direction, channel, "MicrophoneRequest", payload);
      }
      return DecodeAndRecord<aap_protobuf::service::media::source::message::MicrophoneResponse>(
          direction, channel, "MicrophoneResponse", payload);
    case MediaMessageId::MEDIA_MESSAGE_ACK:
      return DecodeAndRecord<aap_protobuf::service::media::source::message::Ack>(
          direction, channel, "Ack", payload);
    default:
      return false;
  }
}

bool TraceInputSourceChannel(ProtoTraceDirection direction,
                             int message_id,
                             const std::string& channel,
                             const DataConstBuffer& payload) {
  if (TraceChannelOpenMessages(direction, message_id, channel, payload)) {
    return true;
  }

  switch (message_id) {
    case InputMessageId::INPUT_MESSAGE_KEY_BINDING_REQUEST:
      return DecodeAndRecord<aap_protobuf::service::media::sink::message::KeyBindingRequest>(
          direction, channel, "KeyBindingRequest", payload);
    case InputMessageId::INPUT_MESSAGE_KEY_BINDING_RESPONSE:
      return DecodeAndRecord<aap_protobuf::service::media::sink::message::KeyBindingResponse>(
          direction, channel, "KeyBindingResponse", payload);
    case InputMessageId::INPUT_MESSAGE_INPUT_REPORT:
      return DecodeAndRecord<aap_protobuf::service::inputsource::message::InputReport>(
          direction, channel, "InputReport", payload);
    default:
      return false;
  }
}

bool TraceBluetoothChannel(ProtoTraceDirection direction,
                           int message_id,
                           const std::string& channel,
                           const DataConstBuffer& payload) {
  if (TraceChannelOpenMessages(direction, message_id, channel, payload)) {
    return true;
  }

  switch (message_id) {
    case BluetoothMessageId::BLUETOOTH_MESSAGE_PAIRING_REQUEST:
      return DecodeAndRecord<aap_protobuf::service::bluetooth::message::BluetoothPairingRequest>(
          direction, channel, "BluetoothPairingRequest", payload);
    case BluetoothMessageId::BLUETOOTH_MESSAGE_PAIRING_RESPONSE:
      return DecodeAndRecord<aap_protobuf::service::bluetooth::message::BluetoothPairingResponse>(
          direction, channel, "BluetoothPairingResponse", payload);
    case BluetoothMessageId::BLUETOOTH_MESSAGE_AUTHENTICATION_RESULT:
      return DecodeAndRecord<aap_protobuf::service::bluetooth::message::BluetoothAuthenticationResult>(
          direction, channel, "BluetoothAuthenticationResult", payload);
    case BluetoothMessageId::BLUETOOTH_MESSAGE_AUTHENTICATION_DATA:
      return DecodeAndRecord<aap_protobuf::service::bluetooth::message::BluetoothAuthenticationData>(
          direction, channel, "BluetoothAuthenticationData", payload);
    default:
      return false;
  }
}

bool TraceWifiProjectionChannel(ProtoTraceDirection direction,
                                int message_id,
                                const std::string& channel,
                                const DataConstBuffer& payload) {
  if (TraceChannelOpenMessages(direction, message_id, channel, payload)) {
    return true;
  }

  switch (message_id) {
    case WifiProjectionMessageId::WIFI_MESSAGE_CREDENTIALS_REQUEST:
      return DecodeAndRecord<aap_protobuf::service::wifiprojection::message::WifiCredentialsRequest>(
          direction, channel, "WifiCredentialsRequest", payload);
    case WifiProjectionMessageId::WIFI_MESSAGE_CREDENTIALS_RESPONSE:
      return DecodeAndRecord<aap_protobuf::service::wifiprojection::message::WifiCredentialsResponse>(
          direction, channel, "WifiCredentialsResponse", payload);
    default:
      return false;
  }
}

bool TraceNavigationStatusChannel(ProtoTraceDirection direction,
                                  int message_id,
                                  const std::string& channel,
                                  const DataConstBuffer& payload) {
  if (TraceChannelOpenMessages(direction, message_id, channel, payload)) {
    return true;
  }

  switch (message_id) {
    case NavigationStatusMessageId::INSTRUMENT_CLUSTER_NAVIGATION_STATUS:
      return DecodeAndRecord<aap_protobuf::service::navigationstatus::message::NavigationStatus>(
          direction, channel, "NavigationStatus", payload);
    case NavigationStatusMessageId::INSTRUMENT_CLUSTER_NAVIGATION_TURN_EVENT:
      return DecodeAndRecord<aap_protobuf::service::navigationstatus::message::NavigationNextTurnEvent>(
          direction, channel, "NavigationTurnEvent", payload);
    case NavigationStatusMessageId::INSTRUMENT_CLUSTER_NAVIGATION_DISTANCE_EVENT:
      return DecodeAndRecord<aap_protobuf::service::navigationstatus::message::NavigationNextTurnDistanceEvent>(
          direction, channel, "NavigationDistanceEvent", payload);
    default:
      return false;
  }
}

bool TraceMediaPlaybackStatusChannel(ProtoTraceDirection direction,
                                     int message_id,
                                     const std::string& channel,
                                     const DataConstBuffer& payload) {
  if (TraceChannelOpenMessages(direction, message_id, channel, payload)) {
    return true;
  }

  switch (message_id) {
    case MediaPlaybackStatusMessageId::MEDIA_PLAYBACK_METADATA:
      return DecodeAndRecord<aap_protobuf::service::mediaplayback::message::MediaPlaybackMetadata>(
          direction, channel, "MediaPlaybackMetadata", payload);
    case MediaPlaybackStatusMessageId::MEDIA_PLAYBACK_STATUS:
      return DecodeAndRecord<aap_protobuf::service::mediaplayback::message::MediaPlaybackStatus>(
          direction, channel, "MediaPlaybackStatus", payload);
    default:
      return false;
  }
}

bool TraceDefaultChannel(ProtoTraceDirection direction,
                         int message_id,
                         const std::string& channel,
                         const DataConstBuffer& payload) {
  return TraceChannelOpenMessages(direction, message_id, channel, payload);
}

}  // namespace

void TraceMessengerMessage(ProtoTraceDirection direction,
                           ChannelId channel_id,
                           const aasdk::messenger::Message& message) {
  const std::string channel_label = ChannelLabel(channel_id);

  if (!ShouldTraceProtoMessages(channel_label)) {
    return;
  }

  if (message.getPayload().size() < aasdk::messenger::MessageId::getSizeOf()) {
    return;
  }

  try {
    aasdk::messenger::MessageId message_id(message.getPayload());
    DataConstBuffer payload(message.getPayload(), message_id.getSizeOf());

    switch (channel_id) {
      case ChannelId::CONTROL:
        TraceControlChannel(direction, message_id.getId(), channel_label, payload);
        break;
      case ChannelId::SENSOR:
        TraceSensorChannel(direction, message_id.getId(), channel_label, payload);
        break;
      case ChannelId::MEDIA_SINK:
      case ChannelId::MEDIA_SINK_VIDEO:
      case ChannelId::MEDIA_SINK_MEDIA_AUDIO:
      case ChannelId::MEDIA_SINK_GUIDANCE_AUDIO:
      case ChannelId::MEDIA_SINK_SYSTEM_AUDIO:
      case ChannelId::MEDIA_SINK_TELEPHONY_AUDIO:
        TraceMediaSinkChannel(direction, message_id.getId(), channel_label, payload);
        break;
      case ChannelId::MEDIA_SOURCE_MICROPHONE:
        TraceMediaSourceChannel(direction, message_id.getId(), channel_label, payload);
        break;
      case ChannelId::INPUT_SOURCE:
        TraceInputSourceChannel(direction, message_id.getId(), channel_label, payload);
        break;
      case ChannelId::BLUETOOTH:
        TraceBluetoothChannel(direction, message_id.getId(), channel_label, payload);
        break;
      case ChannelId::WIFI_PROJECTION:
        TraceWifiProjectionChannel(direction, message_id.getId(), channel_label, payload);
        break;
      case ChannelId::NAVIGATION_STATUS:
        TraceNavigationStatusChannel(direction, message_id.getId(), channel_label, payload);
        break;
      case ChannelId::MEDIA_PLAYBACK_STATUS:
        TraceMediaPlaybackStatusChannel(direction, message_id.getId(), channel_label, payload);
        break;
      default:
        TraceDefaultChannel(direction, message_id.getId(), channel_label, payload);
        break;
    }
  } catch (...) {
    // Ignore malformed payloads.
  }
}

}  // namespace buzz::autoapp::debug
