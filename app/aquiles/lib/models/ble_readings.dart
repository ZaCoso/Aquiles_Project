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

  BleReadings copyWith({
    List<double>? imuLeft,
    bool clearImuLeft = false,
    List<double>? imuRight,
    bool clearImuRight = false,
    double? encoderLeft,
    bool clearEncoderLeft = false,
    double? encoderRight,
    bool clearEncoderRight = false,
    List<double>? forceVector,
    bool clearForceVector = false,
    DateTime? timestamp,
  }) {
    return BleReadings(
      imuLeft: clearImuLeft ? null : imuLeft ?? this.imuLeft,
      imuRight: clearImuRight ? null : imuRight ?? this.imuRight,
      encoderLeft: clearEncoderLeft ? null : encoderLeft ?? this.encoderLeft,
      encoderRight:
          clearEncoderRight ? null : encoderRight ?? this.encoderRight,
      forceVector: clearForceVector ? null : forceVector ?? this.forceVector,
      timestamp: timestamp ?? this.timestamp,
    );
  }
}
