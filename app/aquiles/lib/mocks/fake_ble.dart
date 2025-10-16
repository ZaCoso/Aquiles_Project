import 'dart:async';
import 'dart:collection';
import 'dart:typed_data';

import 'package:aquiles/services/ble_services.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

/// In-memory stand-in for [BleService] used in unit and widget tests.
class FakeBleService implements BleService {
  FakeBleService({
    Iterable<DiscoveredDevice> initialDevices = const [],
    this.autoEmitInitialResults = true,
    this.connectionDelay = Duration.zero,
    Uuid? serviceUuid,
    Uuid? txUuid,
    Uuid? rxUuid,
  })  : serviceUuid = serviceUuid ?? Uuid.parse(_defaultServiceUuid),
        txUuid = txUuid ?? Uuid.parse(_defaultCharacteristicUuid),
        rxUuid = rxUuid ?? Uuid.parse(_defaultCharacteristicUuid) {
    for (final device in initialDevices) {
      _catalog[device.id] = device;
    }
  }

  static const String _defaultServiceUuid = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
  static const String _defaultCharacteristicUuid = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';

  @override
  final Uuid serviceUuid;

  @override
  final Uuid txUuid;

  @override
  final Uuid rxUuid;

  final bool autoEmitInitialResults;
  final Duration connectionDelay;

  final Map<String, DiscoveredDevice> _catalog = <String, DiscoveredDevice>{};
  final Map<String, Queue<bool>> _connectionOutcomes = <String, Queue<bool>>{};
  final StreamController<DiscoveredDevice> _scanController =
      StreamController<DiscoveredDevice>.broadcast();
  final StreamController<String> _notificationController =
      StreamController<String>.broadcast();
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

  /// Optional auto-reply for commands sent via [send].
  String? Function(String command)? autoReply;

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
  void queueConnectionResult({required String deviceId, required bool succeeds}) {
    final queue = _connectionOutcomes.putIfAbsent(deviceId, () => Queue<bool>());
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
    final reply = autoReply?.call(command);
    if (reply != null) {
      emitNotification(reply);
    }
  }

  /// Clears the accumulated command log.
  void clearSentCommands() {
    _sentCommands.clear();
  }

  @override
  Stream<String> getNotificationStream() {
    _ensureActive();
    return _notificationController.stream;
  }

  /// Pushes a notification payload to listeners.
  void emitNotification(String message) {
    if (_notificationController.isClosed) {
      return;
    }
    _notificationController.add(message);
  }

  /// Emits a list of notifications, optionally spacing them out.
  Future<void> emitNotifications(Iterable<String> messages, {Duration delay = Duration.zero}) async {
    for (final message in messages) {
      emitNotification(message);
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
    if (!_notificationController.isClosed) {
      await _notificationController.close();
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
