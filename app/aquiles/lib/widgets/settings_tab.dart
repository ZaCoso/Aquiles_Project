import 'dart:io';

import 'package:aquiles/providers/ble_connection_provider.dart';
import 'package:aquiles/providers/ble_provider.dart';
import 'package:flutter/material.dart';
import 'package:hooks_riverpod/hooks_riverpod.dart';

class SettingsTab extends ConsumerStatefulWidget {
  const SettingsTab({Key? key}) : super(key: key);

  @override
  ConsumerState<SettingsTab> createState() => _SettingsTabState();
}

class _SettingsTabState extends ConsumerState<SettingsTab> {
  double _assistanceLevel = 60;
  double _resistanceLevel = 40;
  double _sensitivityLevel = 80;

  @override
  Widget build(BuildContext context) {
    final ble = ref.watch(bleConnectionProvider.notifier);
    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Container(
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
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            _buildSettingsCard(
              context,
              'Asistencia',
              'Nivel de asistencia al caminar',
              _assistanceLevel,
              (value) {
                setState(() {
                  _assistanceLevel = value;
                });
              },
            ),
            const SizedBox(height: 16),
            _buildSettingsCard(
              context,
              'Resistencia',
              'Nivel de resistencia al movimiento',
              _resistanceLevel,
              (value) {
                setState(() {
                  _resistanceLevel = value;
                });
              },
            ),
            const SizedBox(height: 16),
            _buildSettingsCard(
              context,
              'Sensibilidad',
              'Respuesta a movimientos',
              _sensitivityLevel,
              (value) {
                setState(() {
                  _sensitivityLevel = value;
                });
              },
            ),
            const SizedBox(height: 24),
            SizedBox(
              width: double.infinity,
              child: ElevatedButton(
                onPressed: () async {
                  await ble.sendCommand('AID:${_assistanceLevel.round()}');
                  sleep(Duration(milliseconds: 30));
                  await ble.sendCommand('RES:${_resistanceLevel.round()}');
                  sleep(Duration(milliseconds: 30));
                  await ble.sendCommand('SEN:${_sensitivityLevel.round()}');
                },
                style: ElevatedButton.styleFrom(
                  backgroundColor: const Color(0xFF3F797A),
                  foregroundColor: Colors.white,
                  elevation: 0,
                  padding: const EdgeInsets.symmetric(vertical: 16),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(8),
                  ),
                ),
                child: const Text(
                  'Guardar configuración',
                  style: TextStyle(
                    fontWeight: FontWeight.bold,
                    fontSize: 16,
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildSettingsCard(
    BuildContext context,
    String title,
    String subtitle,
    double value,
    Function(double) onChanged,
  ) {
    return Container(
      padding: const EdgeInsets.all(16),
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
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text(
                title,
                style: Theme.of(context).textTheme.titleMedium,
              ),
              Text(
                '${value.toInt()}%',
                style: const TextStyle(
                  color: Color(0xFF8CCBD5),
                  fontWeight: FontWeight.w500,
                  fontSize: 14,
                ),
              ),
            ],
          ),
          Text(
            subtitle,
            style: TextStyle(
              color: Colors.white.withOpacity(0.7),
              fontSize: 12,
            ),
          ),
          const SizedBox(height: 16),
          Slider(
            value: value,
            min: 0,
            max: 100,
            onChanged: onChanged,
            activeColor: const Color(0xFF8CCBD5),
            inactiveColor: const Color(0xFF3F797A).withOpacity(0.3),
          ),
        ],
      ),
    );
  }
}
