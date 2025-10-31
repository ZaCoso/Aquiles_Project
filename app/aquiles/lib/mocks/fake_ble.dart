import 'dart:async';
import 'dart:collection';
import 'dart:typed_data';

import 'package:aquiles/config/ble_device_config.dart';
import 'package:aquiles/models/ble_readings.dart';
import 'package:aquiles/services/ble_services.dart' show BleClient;
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

/// In-memory stand-in for [BleService] used in unit and widget tests.
class FakeBleService implements BleClient {
  FakeBleService({
    Iterable<DiscoveredDevice> initialDevices = const [],
    this.autoEmitInitialResults = true,
    this.connectionDelay = Duration.zero,
    BleDeviceConfig? config,
  }) : config = config ?? bleDeviceConfig {
    for (final device in initialDevices) {
      _catalog[device.id] = device;
    }
  }

  final BleDeviceConfig config;

  final bool autoEmitInitialResults;
  final Duration connectionDelay;

  final Map<String, DiscoveredDevice> _catalog = <String, DiscoveredDevice>{};
  final Map<String, Queue<bool>> _connectionOutcomes = <String, Queue<bool>>{};
  final StreamController<DiscoveredDevice> _scanController =
      StreamController<DiscoveredDevice>.broadcast();
  final StreamController<BleReadings> _telemetryController =
      StreamController<BleReadings>.broadcast();
  final List<String> _sentCommands = <String>[];

  String? _connectedDeviceId;
  bool _initialResultsEmitted = false;
  bool _isScanning = false;
  bool _disposed = false;

  /// Notifies when a connection succeeds.
  void Function(String deviceId)? onConnect;

  /// Notifies when [disconnect] is called.
  void Function(String deviceId)? onDisconnect;

  /// Notifies when [send] is invoked.
  void Function(String command)? onSend;

  /// Optional auto-generated telemetry for commands sent via [send].
  BleReadings? Function(String command)? autoTelemetry;

  bool get isConnected => _connectedDeviceId != null;

  bool get isScanning => _isScanning;

  String? get connectedDeviceId => _connectedDeviceId;

  DiscoveredDevice? get connectedDevice =>
      _connectedDeviceId == null ? null : _catalog[_connectedDeviceId];

  List<String> get sentCommands => List.unmodifiable(_sentCommands);

  Iterable<DiscoveredDevice> get knownDevices =>
      List<DiscoveredDevice>.unmodifiable(_catalog.values);

  @override
  Stream<DiscoveredDevice> scan() {
    _ensureActive();
    _isScanning = true;

    if (autoEmitInitialResults && !_initialResultsEmitted) {
      _initialResultsEmitted = true;
      Future<void>.microtask(() {
        if (_scanController.isClosed) {
          return;
        }
        for (final device in _catalog.values) {
          _scanController.add(device);
        }
      });
    }

    return _scanController.stream;
  }

  /// Adds a discovered device to the current scan stream.
  void emitDiscoveredDevice(DiscoveredDevice device) {
    _ensureActive();
    _catalog[device.id] = device;
    if (_scanController.isClosed) {
      return;
    }
    if (_scanController.hasListener) {
      _scanController.add(device);
    }
  }

  /// Emits an error to subscribers of [scan].
  void emitScanError(Object error, [StackTrace? stackTrace]) {
    if (_scanController.isClosed) {
      return;
    }
    _scanController.addError(error, stackTrace);
  }

  /// Stops the active scan. Set [closeStream] to true to complete listeners.
  void stopScan({bool closeStream = false}) {
    _isScanning = false;
    if (closeStream && !_scanController.isClosed) {
      _scanController.close();
    }
  }

  /// Queues deterministic results for [connectToDeviceId].
  void queueConnectionResult(
      {required String deviceId, required bool succeeds}) {
    final queue =
        _connectionOutcomes.putIfAbsent(deviceId, () => Queue<bool>());
    queue.addLast(succeeds);
  }

  /// Clears queued outcomes for [deviceId], or for all devices when null.
  void clearQueuedResults([String? deviceId]) {
    if (deviceId == null) {
      _connectionOutcomes.clear();
      return;
    }
    _connectionOutcomes.remove(deviceId);
  }

  @override
  Future<bool> connectToDeviceId(String deviceId) async {
    _ensureActive();
    if (connectionDelay > Duration.zero) {
      await Future<void>.delayed(connectionDelay);
    }

    final queue = _connectionOutcomes[deviceId];
    final bool succeeds;
    if (queue != null && queue.isNotEmpty) {
      succeeds = queue.removeFirst();
    } else {
      succeeds = _catalog.containsKey(deviceId);
    }

    if (!succeeds) {
      return false;
    }

    _connectedDeviceId = deviceId;
    onConnect?.call(deviceId);
    return true;
  }

  @override
  Future<bool> connectToDevice(String nameToFind) async {
    _ensureActive();
    final targetName = nameToFind.toLowerCase();
    try {
      final device = _catalog.values.firstWhere(
        (entry) => entry.name.toLowerCase() == targetName,
      );
      return connectToDeviceId(device.id);
    } on StateError {
      return false;
    }
  }

  @override
  Future<void> send(String command) async {
    _ensureActive();
    if (!isConnected) {
      throw StateError('Cannot send command when no device is connected.');
    }
    _sentCommands.add(command);
    onSend?.call(command);
    final reply = autoTelemetry?.call(command);
    if (reply != null) {
      emitTelemetry(reply);
    }
  }

  /// Clears the accumulated command log.
  void clearSentCommands() {
    _sentCommands.clear();
  }

  @override
  Stream<BleReadings> telemetryStream() {
    _ensureActive();
    return _telemetryController.stream;
  }

  /// Pushes a telemetry payload to listeners.
  void emitTelemetry(BleReadings telemetry) {
    if (_telemetryController.isClosed) {
      return;
    }
    _telemetryController.add(telemetry);
  }

  /// Emits a list of telemetry frames, optionally spacing them out.
  Future<void> emitTelemetries(Iterable<BleReadings> frames,
      {Duration delay = Duration.zero}) async {
    for (final telemetry in frames) {
      emitTelemetry(telemetry);
      if (delay > Duration.zero) {
        await Future<void>.delayed(delay);
      }
    }
  }

  @override
  Future<void> disconnect() async {
    if (!isConnected) {
      return;
    }
    final deviceId = _connectedDeviceId!;
    _connectedDeviceId = null;
    onDisconnect?.call(deviceId);
  }

  /// Closes underlying streams. No further operations should be executed afterwards.
  Future<void> dispose() async {
    if (_disposed) {
      return;
    }
    _disposed = true;
    _isScanning = false;
    _connectedDeviceId = null;

    if (!_scanController.isClosed) {
      await _scanController.close();
    }
    if (!_telemetryController.isClosed) {
      await _telemetryController.close();
    }
  }

  /// Allows emitting initial results again if needed after tests reset state.
  void resetInitialEmission() {
    _initialResultsEmitted = false;
  }

  static DiscoveredDevice createDevice({
    required String id,
    required String name,
    int rssi = -55,
    List<Uuid> serviceUuids = const <Uuid>[],
    Map<Uuid, Uint8List> serviceData = const <Uuid, Uint8List>{},
    Uint8List? manufacturerData,
    Connectable connectable = Connectable.available,
  }) {
    return DiscoveredDevice(
      id: id,
      name: name,
      serviceData: serviceData,
      manufacturerData: manufacturerData ?? Uint8List(0),
      rssi: rssi,
      serviceUuids: serviceUuids,
      connectable: connectable,
    );
  }

  void _ensureActive() {
    if (_disposed) {
      throw StateError('FakeBleService has been disposed.');
    }
  }
}
