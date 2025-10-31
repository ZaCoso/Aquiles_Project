import 'package:aquiles/config/ble_device_config.dart';

class BleReadings {
  const BleReadings({
    this.imuLeft,
    this.imuRight,
    this.encoderLeft,
    this.encoderRight,
    this.forceVector,
    required this.timestamp,
  });

  final List<double>? imuLeft;
  final List<double>? imuRight;
  final double? encoderLeft;
  final double? encoderRight;
  final List<double>? forceVector;
  final DateTime timestamp;

  static BleReadings empty() =>
      BleReadings(timestamp: DateTime.fromMillisecondsSinceEpoch(0));

  BleReadings update(
    BleSensorType sensor,
    List<double> values,
    DateTime newTimestamp,
  ) {
    switch (sensor) {
      case BleSensorType.imuLeft:
        return BleReadings(
          imuLeft: values,
          imuRight: imuRight,
          encoderLeft: encoderLeft,
          encoderRight: encoderRight,
          forceVector: forceVector,
          timestamp: newTimestamp,
        );
      case BleSensorType.imuRight:
        return BleReadings(
          imuLeft: imuLeft,
          imuRight: values,
          encoderLeft: encoderLeft,
          encoderRight: encoderRight,
          forceVector: forceVector,
          timestamp: newTimestamp,
        );
      case BleSensorType.encoderLeft:
        return BleReadings(
          imuLeft: imuLeft,
          imuRight: imuRight,
          encoderLeft: values.isNotEmpty ? values.first : null,
          encoderRight: encoderRight,
          forceVector: forceVector,
          timestamp: newTimestamp,
        );
      case BleSensorType.encoderRight:
        return BleReadings(
          imuLeft: imuLeft,
          imuRight: imuRight,
          encoderLeft: encoderLeft,
          encoderRight: values.isNotEmpty ? values.first : null,
          forceVector: forceVector,
          timestamp: newTimestamp,
        );
      case BleSensorType.forceVector:
        return BleReadings(
          imuLeft: imuLeft,
          imuRight: imuRight,
          encoderLeft: encoderLeft,
          encoderRight: encoderRight,
          forceVector: values,
          timestamp: newTimestamp,
        );
    }
  }
}
