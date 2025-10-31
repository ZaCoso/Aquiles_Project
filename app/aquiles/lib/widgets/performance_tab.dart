import 'package:aquiles/config/ble_device_config.dart';
import 'package:aquiles/models/ble_readings.dart';
import 'package:aquiles/providers/ble_connection_provider.dart';
import 'package:aquiles/providers/ble_provider.dart';
import 'package:flutter/material.dart';
import 'dart:math';

import 'package:flutter_riverpod/flutter_riverpod.dart';

class PerformanceTab extends ConsumerWidget {
  const PerformanceTab({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final AsyncValue<BleReadings> telemetry =
        ref.watch(telemetryStreamProvider);
    final bleState = ref.watch(bleConnectionProvider);

    if (bleState == BleConnectionState.connected) {
      return SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            _buildPerformanceCard(context),
            telemetry.when(
              data: (data) => Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  _buildTelemetrySection(context, data),
                  const SizedBox(height: 16),
                  _buildDebugTelemetryLabel(data),
                ],
              ),
              loading: () => const CircularProgressIndicator(),
              error: (e, _) => Text("Error: $e"),
            ),
          ],
        ),
      );
    } else {
      return const Center(child: Text('Desconectado'));
    }
  }

  Widget _buildPerformanceCard(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [
            const Color(0xFF0A1128),
            const Color(0xFF295B62).withOpacity(0.5),
          ],
        ),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(
          color: const Color(0xFF3F797A).withOpacity(0.3),
        ),
      ),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Análisis de Rendimiento',
              style: Theme.of(context).textTheme.titleLarge?.copyWith(
                    fontSize: 18,
                  ),
            ),
            const SizedBox(height: 16),
            _buildProgressBar(context, 'Eficiencia', 87),
            const SizedBox(height: 16),
            _buildProgressBar(context, 'Respuesta', 92),
            const SizedBox(height: 16),
            _buildProgressBar(context, 'Estabilidad', 78),
            const SizedBox(height: 24),
            _buildUsageHistoryCard(context),
          ],
        ),
      ),
    );
  }

  Widget _buildProgressBar(BuildContext context, String label, int value) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text(
              label,
              style: TextStyle(
                color: Colors.white.withOpacity(0.7),
                fontSize: 14,
              ),
            ),
            Text(
              '$value%',
              style: const TextStyle(
                color: Color(0xFF8CCBD5),
                fontSize: 14,
                fontWeight: FontWeight.w500,
              ),
            ),
          ],
        ),
        const SizedBox(height: 8),
        Container(
          height: 8,
          decoration: BoxDecoration(
            color: const Color(0xFF0A1128),
            borderRadius: BorderRadius.circular(4),
          ),
          child: Stack(
            children: [
              FractionallySizedBox(
                widthFactor: value / 100,
                child: Container(
                  decoration: BoxDecoration(
                    gradient: const LinearGradient(
                      colors: [Color(0xFF5E9F9F), Color(0xFF8CCBD5)],
                    ),
                    borderRadius: BorderRadius.circular(4),
                  ),
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _buildUsageHistoryCard(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: const Color(0xFF0A1128).withOpacity(0.8),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: const Color(0xFF3F797A).withOpacity(0.3),
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(
                Icons.bar_chart,
                color: Color(0xFF8CCBD5),
                size: 18,
              ),
              const SizedBox(width: 8),
              Text(
                'Historial de Uso',
                style: Theme.of(context).textTheme.titleMedium,
              ),
            ],
          ),
          const SizedBox(height: 16),
          SizedBox(
            height: 120,
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              crossAxisAlignment: CrossAxisAlignment.end,
              children: List.generate(
                7,
                (index) {
                  final height = 30 + Random().nextInt(60);
                  return Container(
                    width: 24,
                    height: height.toDouble(),
                    decoration: BoxDecoration(
                      gradient: const LinearGradient(
                        begin: Alignment.bottomCenter,
                        end: Alignment.topCenter,
                        colors: [Color(0xFF8CCBD5), Color(0xFF5E9F9F)],
                      ),
                      borderRadius: BorderRadius.circular(4),
                    ),
                  );
                },
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildTelemetrySection(BuildContext context, BleReadings telemetry) {
    final textStyle = Theme.of(context).textTheme.bodyMedium?.copyWith(
          color: Colors.white.withOpacity(0.8),
        );
    final highlightStyle = Theme.of(context).textTheme.bodyLarge?.copyWith(
          color: const Color(0xFF8CCBD5),
          fontWeight: FontWeight.w600,
        );

    Widget buildRow(String label, String value) {
      return Padding(
        padding: const EdgeInsets.symmetric(vertical: 4),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text(label, style: textStyle),
            Text(value, style: highlightStyle),
          ],
        ),
      );
    }

    return Container(
      margin: const EdgeInsets.only(top: 16),
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xFF0A1128).withOpacity(0.7),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: const Color(0xFF3F797A).withOpacity(0.3),
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            'Telemetría en vivo',
            style: Theme.of(context).textTheme.titleMedium,
          ),
          const SizedBox(height: 12),
          if (telemetry.timestamp.millisecondsSinceEpoch != 0)
            buildRow(
              'Actualización',
              telemetry.timestamp.toLocal().toIso8601String(),
            ),
          buildRow(
            bleDeviceConfig.sensorForType(BleSensorType.imuLeft).label,
            _formatList(telemetry.imuLeft),
          ),
          buildRow(
            bleDeviceConfig.sensorForType(BleSensorType.imuRight).label,
            _formatList(telemetry.imuRight),
          ),
          buildRow(
            bleDeviceConfig.sensorForType(BleSensorType.encoderLeft).label,
            telemetry.encoderLeft?.toStringAsFixed(3) ?? '--',
          ),
          buildRow(
            bleDeviceConfig.sensorForType(BleSensorType.encoderRight).label,
            telemetry.encoderRight?.toStringAsFixed(3) ?? '--',
          ),
          buildRow(
            bleDeviceConfig.sensorForType(BleSensorType.forceVector).label,
            _formatList(telemetry.forceVector),
          ),
        ],
      ),
    );
  }

  String _formatList(List<double>? values) {
    if (values == null || values.isEmpty) {
      return '--';
    }
    return values.map((v) => v.toStringAsFixed(3)).join(', ');
  }

  Widget _buildDebugTelemetryLabel(BleReadings telemetry) {
    final buffer = StringBuffer();
    buffer.writeln('Última actualización: '
        '${telemetry.timestamp.toLocal().toIso8601String()}');
    if (telemetry.imuLeft != null) {
      buffer.writeln('IMU L: ${_formatList(telemetry.imuLeft)}');
    }
    if (telemetry.imuRight != null) {
      buffer.writeln('IMU R: ${_formatList(telemetry.imuRight)}');
    }
    if (telemetry.encoderLeft != null) {
      buffer.writeln('Encoder L: ${telemetry.encoderLeft!.toStringAsFixed(3)}');
    }
    if (telemetry.encoderRight != null) {
      buffer
          .writeln('Encoder R: ${telemetry.encoderRight!.toStringAsFixed(3)}');
    }
    if (telemetry.forceVector != null) {
      buffer.writeln('Fuerzas: ${_formatList(telemetry.forceVector)}');
    }

    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Colors.black.withOpacity(0.4),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: Colors.white24),
      ),
      child: Text(
        buffer.toString().trim(),
        style: const TextStyle(
          fontFamily: 'monospace',
          fontSize: 12,
          color: Colors.white,
        ),
      ),
    );
  }
}
