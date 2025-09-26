// lib/providers/ble_connection_provider.dart
import 'package:aquiles/services/ble_services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'ble_provider.dart';

enum BleConnectionState {
  disconnected,
  connecting,
  connected,
  error,
}

class BleConnectionNotifier extends StateNotifier<BleConnectionState> {
  BleConnectionNotifier(this._bleService)
      : super(BleConnectionState.disconnected);

  final BleService _bleService;

  Future<void> connect() async {
    state = BleConnectionState.connecting;
    try {
      final success = await _bleService.connectToDevice("Aquiles X1");
      state = success ? BleConnectionState.connected : BleConnectionState.error;
    } catch (_) {
      state = BleConnectionState.error;
    }
  }

  Future<bool> connectToId(String deviceId) async {
    state = BleConnectionState.connecting;
    try {
      final success = await _bleService.connectToDeviceId(deviceId);
      state = success ? BleConnectionState.connected : BleConnectionState.error;
      return success;
    } catch (_) {
      state = BleConnectionState.error;
      return false;
    }
  }

  Future<void> disconnect() async {
    await _bleService.disconnect();
    state = BleConnectionState.disconnected;
  }

  Future<void> sendCommand(String cmd) async {
    if (state == BleConnectionState.connected) {
      await _bleService.send(cmd);
    }
  }
}

final bleConnectionProvider =
    StateNotifierProvider<BleConnectionNotifier, BleConnectionState>(
  (ref) => BleConnectionNotifier(ref.watch(bleServiceProvider)),
);
