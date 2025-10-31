import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:aquiles/config/ble_device_config.dart';
import 'package:aquiles/models/ble_readings.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

abstract class BleClient {
  Stream<DiscoveredDevice> scan();
  Stream<BleReadings> telemetryStream();
  Future<bool> connectToDeviceId(String deviceId);
  Future<bool> connectToDevice(String nameToFind);
  Future<void> send(String command);
  Future<void> disconnect();
}

class BleService implements BleClient {
  BleService(this._ble, {required this.config});

  final FlutterReactiveBle _ble;
  final BleDeviceConfig config;

  DiscoveredDevice? _device;
  QualifiedCharacteristic? _commandChar;
  StreamSubscription<ConnectionStateUpdate>? _connectionSubscription;
  final Map<BleSensorType, StreamSubscription<List<int>>>
      _notificationSubscriptions =
      <BleSensorType, StreamSubscription<List<int>>>{};

  BleReadings _latestReadings = BleReadings.empty();
  final StreamController<BleReadings> _readingsController =
      StreamController<BleReadings>.broadcast();

  @override
  Stream<DiscoveredDevice> scan() {
    return _ble.scanForDevices(withServices: [config.serviceUuid]);
  }

  @override
  Stream<BleReadings> telemetryStream() => _readingsController.stream;

  @override
  Future<bool> connectToDeviceId(String deviceId) async {
    final completer = Completer<bool>();
    _connectionSubscription = _ble.connectToDevice(
      id: deviceId,
      servicesWithCharacteristicsToDiscover: {
        config.serviceUuid:
            config.sensors.map((sensor) => sensor.characteristicUuid).toList()
              ..add(config.commandUuid),
      },
    ).listen(
      (event) async {
        switch (event.connectionState) {
          case DeviceConnectionState.connected:
            await _onConnected(deviceId);
            if (!completer.isCompleted) {
              completer.complete(true);
            }
            break;
          case DeviceConnectionState.disconnected:
            if (!completer.isCompleted) {
              completer.complete(false);
            }
            break;
          default:
            break;
        }
      },
      onError: (error, stackTrace) {
        if (!completer.isCompleted) {
          completer.completeError(error, stackTrace);
        }
      },
    );
    return completer.future;
  }

  @override
  Future<bool> connectToDevice(String nameToFind) async {
    final scanStream = _ble.scanForDevices(withServices: [config.serviceUuid]);
    await for (final device in scanStream) {
      if (device.name == nameToFind) {
        _device = device;
        return connectToDeviceId(device.id);
      }
    }
    return false;
  }

  @override
  Future<void> send(String command) async {
    if (_commandChar == null) {
      throw StateError('No hay característica de comando disponible.');
    }
    await _ble.writeCharacteristicWithResponse(
      _commandChar!,
      value: utf8.encode(command),
    );
  }

  @override
  Future<void> disconnect() async {
    await _cancelNotificationSubscriptions();

    try {
      await _connectionSubscription?.cancel();
    } catch (_) {}
    _connectionSubscription = null;

    _commandChar = null;
    _device = null;
    _latestReadings = BleReadings.empty();
  }

  Future<void> dispose() async {
    await disconnect();
    await _readingsController.close();
  }

  Future<void> _onConnected(String deviceId) async {
    await _cancelNotificationSubscriptions();
    _latestReadings = BleReadings.empty();

    List<DiscoveredService> services;
    try {
      services = await _ble.discoverServices(deviceId);
    } catch (e) {
      print('[BLE] No se pudieron descubrir servicios: $e');
      return;
    }

    for (final service in services) {
      print('Service: ${service.serviceId}');
      for (final char in service.characteristics) {
        print('Characteristic: ${char.characteristicId}');
      }
    }

    DiscoveredService? targetService;
    for (final service in services) {
      if (service.serviceId == config.serviceUuid) {
        targetService = service;
        break;
      }
    }

    if (targetService == null) {
      print(
          '[BLE] Servicio ${config.serviceUuid} no encontrado en el dispositivo.');
      return;
    }

    final commandCharacteristic = _findCharacteristic(
      targetService,
      config.commandUuid,
    );
    _commandChar = commandCharacteristic == null
        ? null
        : QualifiedCharacteristic(
            deviceId: deviceId,
            serviceId: commandCharacteristic.serviceId,
            characteristicId: commandCharacteristic.characteristicId,
          );

    for (final sensor in config.sensors) {
      await _subscribeToSensor(
        deviceId: deviceId,
        service: targetService,
        sensor: sensor,
      );
    }

    try {
      final negotiatedMtu = await _ble.requestMtu(deviceId: deviceId, mtu: 185);
      print('[BLE] MTU negociado: $negotiatedMtu');
    } catch (e) {
      print('[BLE] No se pudo negociar MTU alto: $e');
    }
  }

  Future<void> _subscribeToSensor({
    required String deviceId,
    required DiscoveredService service,
    required BleSensorConfig sensor,
  }) async {
    final characteristic = _findCharacteristic(
      service,
      sensor.characteristicUuid,
    );
    if (characteristic == null) {
      print(
          '[BLE] Característica ${sensor.label} (${sensor.characteristicUuid}) no encontrada.');
      return;
    }

    final qualified = QualifiedCharacteristic(
      deviceId: deviceId,
      serviceId: characteristic.serviceId,
      characteristicId: characteristic.characteristicId,
    );

    await _notificationSubscriptions[sensor.type]?.cancel();
    _notificationSubscriptions[sensor.type] = _ble
        .subscribeToCharacteristic(qualified)
        .listen(
          (data) => _handleSensorData(sensor, data),
          onError: (error, stackTrace) =>
              print('[BLE] Error en notificación (${sensor.label}): $error'),
          onDone: () =>
              print('[BLE] Notificaciones terminadas (${sensor.label})'),
          cancelOnError: true,
        );
  }

  DiscoveredCharacteristic? _findCharacteristic(
    DiscoveredService service,
    Uuid characteristicId,
  ) {
    for (final characteristic in service.characteristics) {
      if (characteristic.characteristicId == characteristicId) {
        return characteristic;
      }
    }
    return null;
  }

  Future<void> _cancelNotificationSubscriptions() async {
    for (final entry in _notificationSubscriptions.entries) {
      try {
        await entry.value.cancel();
      } catch (e) {
        print('[BLE] Error al cancelar notificación (${entry.key}): $e');
      }
    }
    _notificationSubscriptions.clear();
  }

  void _handleSensorData(BleSensorConfig sensor, List<int> rawData) {
    if (rawData.isEmpty) {
      return;
    }

    final values = _floatsFromBytes(rawData);
    if (sensor.expectedValues != null &&
        sensor.expectedValues != values.length) {
      print('[BLE] Valores inesperados para ${sensor.label}: '
          'se esperaban ${sensor.expectedValues}, se recibieron ${values.length}');
    }
    final timestamp = DateTime.now();
    _latestReadings = _latestReadings.update(sensor.type, values, timestamp);
    if (!_readingsController.isClosed) {
      _readingsController.add(_latestReadings);
    }
  }

  List<double> _floatsFromBytes(List<int> data) {
    if (data.isEmpty) {
      return const <double>[];
    }
    final byteLength = data.length;
    if (byteLength % 4 != 0) {
      print('[BLE] Longitud inválida (${data.length}) para flotantes.');
    }
    final count = byteLength ~/ 4;
    final view = ByteData.sublistView(Uint8List.fromList(data));
    final values = <double>[];
    for (var i = 0; i < count; i++) {
      values.add(view.getFloat32(i * 4, Endian.little));
    }
    return values;
  }
}
