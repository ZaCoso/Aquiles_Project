import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

enum BleSensorType {
  imuLeft,
  imuRight,
  encoderLeft,
  encoderRight,
  forceVector,
}

class BleSensorConfig {
  BleSensorConfig({
    required this.type,
    required this.characteristicUuid,
    required this.label,
    this.expectedValues,
  });

  final BleSensorType type;
  final Uuid characteristicUuid;
  final String label;
  final int? expectedValues;
}

class BleDeviceConfig {
  BleDeviceConfig({
    required this.deviceName,
    required this.serviceUuid,
    required this.commandUuid,
    required this.sensors,
  });

  final String deviceName;
  final Uuid serviceUuid;
  final Uuid commandUuid;
  final List<BleSensorConfig> sensors;

  BleSensorConfig sensorForType(BleSensorType type) {
    return sensors.firstWhere(
      (sensor) => sensor.type == type,
      orElse: () => throw StateError('Sensor $type no configurado'),
    );
  }
}

final BleDeviceConfig bleDeviceConfig = BleDeviceConfig(
  deviceName: 'Aquiles x1',
  serviceUuid: Uuid.parse('6e400010-b5a3-f393-e0a9-e50e24dcca9e'),
  commandUuid: Uuid.parse('6e400020-b5a3-f393-e0a9-e50e24dcca9e'),
  sensors: [
    BleSensorConfig(
      type: BleSensorType.imuLeft,
      characteristicUuid: Uuid.parse('6e400021-b5a3-f393-e0a9-e50e24dcca9e'),
      label: 'IMU izquierda',
      expectedValues: 6,
    ),
    BleSensorConfig(
      type: BleSensorType.imuRight,
      characteristicUuid: Uuid.parse('6e400022-b5a3-f393-e0a9-e50e24dcca9e'),
      label: 'IMU derecha',
      expectedValues: 6,
    ),
    BleSensorConfig(
      type: BleSensorType.encoderLeft,
      characteristicUuid: Uuid.parse('6e400023-b5a3-f393-e0a9-e50e24dcca9e'),
      label: 'Encoder izquierdo',
      expectedValues: 1,
    ),
    BleSensorConfig(
      type: BleSensorType.encoderRight,
      characteristicUuid: Uuid.parse('6e400024-b5a3-f393-e0a9-e50e24dcca9e'),
      label: 'Encoder derecho',
      expectedValues: 1,
    ),
    BleSensorConfig(
      type: BleSensorType.forceVector,
      characteristicUuid: Uuid.parse('6e400025-b5a3-f393-e0a9-e50e24dcca9e'),
      label: 'Vector de fuerzas',
      expectedValues: 3,
    ),
  ],
);
