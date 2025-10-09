// lib/providers/ble_provider.dart
import 'dart:async';
import 'dart:typed_data';
import 'package:aquiles/mocks/fake_ble.dart';
import 'package:aquiles/services/ble_services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

final flutterBleProvider = Provider<FlutterReactiveBle>((ref) {
  return FlutterReactiveBle();
});

final bleServiceProvider = Provider<BleService>((ref) => FakeBleService(
      initialDevices: [
        DiscoveredDevice(
          id: '00:11:22:33:44:55',
          name: 'Aquiles X1',
          serviceData: {},
          manufacturerData: Uint8List.fromList([
            72,
            101,
            108,
            108,
            111
          ]), // Uint8List.fromList([72, 101, 108, 108, 111]),
          rssi: -50,
          serviceUuids: [],
        ),
        DiscoveredDevice(
          id: '66:77:88:99:AA:BB',
          name: 'Aquiles X1',
          serviceData: {},
          manufacturerData: Uint8List(0),
          rssi: -60,
          serviceUuids: [],
        ),
      ],
      serviceUuid: Uuid.parse("6e400001-b5a3-f393-e0a9-e50e24dcca9e"),
      txUuid: Uuid.parse("6e400002-b5a3-f393-e0a9-e50e24dcca9e"),
      rxUuid: Uuid.parse("6e400002-b5a3-f393-e0a9-e50e24dcca9e"),
    )); // Provider<FakeBleService>
// Provider<BleService>((ref) {
//   final ble = ref.watch(flutterBleProvider);
//   return BleService(
//     ble,
//     Uuid.parse("6e400001-b5a3-f393-e0a9-e50e24dcca9e"), // service UUID
//     Uuid.parse("6e400002-b5a3-f393-e0a9-e50e24dcca9e"), // TX (App -> ESP32)
//     Uuid.parse(
//         "6e400002-b5a3-f393-e0a9-e50e24dcca9e"), // RX (ESP32 -> App) - SAME UUID!
//   );
// });

final notificationStreamProvider = StreamProvider<String>((ref) {
  final ble = ref.watch(bleServiceProvider);
  return ble.getNotificationStream();
});

class BleScanState {
  final bool scanning;
  final List<DiscoveredDevice> devices;
  final String? error;
  final bool showIndicator;

  BleScanState({
    required this.scanning,
    required this.devices,
    this.error,
    this.showIndicator = false,
  });

  BleScanState copyWith({
    bool? scanning,
    List<DiscoveredDevice>? devices,
    String? error,
    bool? showIndicator,
  }) =>
      BleScanState(
        scanning: scanning ?? this.scanning,
        devices: devices ?? this.devices,
        error: error,
        showIndicator: showIndicator ?? this.showIndicator,
      );
}

class BleScanNotifier extends StateNotifier<BleScanState> {
  BleScanNotifier(this._bleService)
      : super(BleScanState(scanning: false, devices: const []));

  final BleService _bleService;
  StreamSubscription<DiscoveredDevice>? _scanSub;
  Timer? _pruneTimer;

  // Track last seen timestamp to remove stale devices (e.g., turned off)
  final Map<String, _TrackedDevice> _tracked = {};
  static const Duration _staleAfter = Duration(seconds: 10);

  Future<void> startScan() async {
    await _scanSub?.cancel();
    _pruneTimer?.cancel();
    _tracked.clear();
    state = state.copyWith(scanning: true, devices: [], showIndicator: true);
    print('[BLE] Starting scan...');
    _scanSub = _bleService.scan().listen(
      (device) {
        final now = DateTime.now();
        final firstTime = !_tracked.containsKey(device.id);
        _tracked[device.id] = _TrackedDevice(device: device, lastSeen: now);
        if (firstTime) {
          final name = device.name.isNotEmpty ? device.name : 'Dispositivo BLE';
          print('[BLE] Found: ' +
              name +
              ' (' +
              device.id +
              ') RSSI ' +
              device.rssi.toString());
        }
        final updated = _tracked.values.map((e) => e.device).toList();
        state = state.copyWith(devices: updated);
        final summary = updated
            .map((d) =>
                (d.name.isNotEmpty ? d.name : 'Dispositivo BLE') +
                ' (' +
                d.id +
                ')')
            .join(', ');
        print('[BLE] Devices so far (' +
            updated.length.toString() +
            '): ' +
            summary);
      },
      onError: (e) {
        state = state.copyWith(
            scanning: false, error: e.toString(), showIndicator: false);
        print('[BLE] Scan error: ' + e.toString());
      },
    );

    // Hide the searching indicator after 15 seconds, even if still scanning
    Future.delayed(const Duration(seconds: 15), () {
      if (!mounted) return;
      if (state.scanning) {
        state = state.copyWith(showIndicator: false);
      }
    });

    // Periodically prune devices not seen recently (e.g., turned off)
    _pruneTimer = Timer.periodic(const Duration(seconds: 3), (_) {
      if (!mounted || !state.scanning) return;
      final now = DateTime.now();
      final toRemove = <String>[];
      _tracked.forEach((id, info) {
        if (now.difference(info.lastSeen) > _staleAfter) {
          toRemove.add(id);
        }
      });
      if (toRemove.isNotEmpty) {
        for (final id in toRemove) {
          final dev = _tracked.remove(id)?.device;
          final name =
              (dev?.name.isNotEmpty ?? false) ? dev!.name : 'Dispositivo BLE';
          print('[BLE] Pruned stale device (no recent adverts): ' +
              name +
              ' (' +
              id +
              ')');
        }
        final updated = _tracked.values.map((e) => e.device).toList();
        state = state.copyWith(devices: updated);
      }
    });
  }

  Future<void> stopScan() async {
    await _scanSub?.cancel();
    _scanSub = null;
    _pruneTimer?.cancel();
    _pruneTimer = null;
    state = state.copyWith(scanning: false, showIndicator: false);
    final list = state.devices
        .map((d) =>
            (d.name.isNotEmpty ? d.name : 'Dispositivo BLE') +
            ' (' +
            d.id +
            ')')
        .join(', ');
    print('[BLE] Stopped scan. Devices found (' +
        state.devices.length.toString() +
        '): ' +
        list);
  }

  void removeDevice(String deviceId) {
    final updated = state.devices.where((d) => d.id != deviceId).toList();
    state = state.copyWith(devices: updated);
    print('[BLE] Removed device from list: ' + deviceId);
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    _pruneTimer?.cancel();
    super.dispose();
  }
}

final bleScanProvider =
    StateNotifierProvider<BleScanNotifier, BleScanState>((ref) {
  return BleScanNotifier(ref.watch(bleServiceProvider));
});

class _TrackedDevice {
  _TrackedDevice({required this.device, required this.lastSeen});
  final DiscoveredDevice device;
  DateTime lastSeen;
}
