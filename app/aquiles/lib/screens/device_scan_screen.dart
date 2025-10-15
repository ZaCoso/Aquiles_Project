import 'dart:async';

import 'package:aquiles/providers/ble_connection_provider.dart';
import 'package:aquiles/providers/ble_provider.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

class DeviceScanScreen extends ConsumerStatefulWidget {
  const DeviceScanScreen({super.key});

  @override
  ConsumerState<DeviceScanScreen> createState() => _DeviceScanScreenState();
}

class _DeviceScanScreenState extends ConsumerState<DeviceScanScreen> {
  static const Duration _scanDuration = Duration(seconds: 30);

  Timer? _timer;
  int _secondsLeft = _scanDuration.inSeconds;

  @override
  void initState() {
    super.initState();
    // Defer the initial scan until after the first frame so the context exists.
    WidgetsBinding.instance.addPostFrameCallback((_) => _startScan());
  }

  @override
  void dispose() {
    _timer?.cancel();
    ref.read(bleScanProvider.notifier).stopScan();
    super.dispose();
  }

  void _startScan() {
    _timer?.cancel();
    setState(() {
      _secondsLeft = _scanDuration.inSeconds;
    });
    ref.read(bleScanProvider.notifier).startScan();

    _timer = Timer.periodic(const Duration(seconds: 1), (timer) {
      if (!mounted) return;
      if (_secondsLeft <= 1) {
        timer.cancel();
        ref.read(bleScanProvider.notifier).stopScan();
        setState(() {
          _secondsLeft = 0;
        });
      } else {
        setState(() {
          _secondsLeft -= 1;
        });
      }
    });
  }

  Future<void> _retryScan() async {
    await ref.read(bleScanProvider.notifier).stopScan();
    _startScan();
  }

  Future<void> _connectToDevice(DiscoveredDevice device) async {
    await ref.read(bleScanProvider.notifier).stopScan();
    final notifier = ref.read(bleConnectionProvider.notifier);
    final success = await notifier.connectToId(device.id);
    if (!mounted) return;
    if (!success) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            'No se pudo conectar con ${device.name.isNotEmpty ? device.name : 'el dispositivo'}.',
          ),
          backgroundColor: Colors.red.shade400,
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final scanState = ref.watch(bleScanProvider);
    final bleState = ref.watch(bleConnectionProvider);

    final isScanning = scanState.scanning;
    final hasDevices = scanState.devices.isNotEmpty;
    final isConnecting = bleState == BleConnectionState.connecting;
    final title = isScanning
        ? 'Buscando dispositivos cercanos (${_secondsLeft}s)'
        : hasDevices
            ? 'Selecciona un dispositivo'
            : 'No se encontraron dispositivos';
    final description = isScanning
        ? 'Manténgase cerca de los dispositivos y asegúrese de que esté colocado, encendido el exoesqueleto y el bluetooth.'
        : hasDevices
            ? 'Elige un dispositivo para conectarte con el exoesqueleto.'
            : 'No pudimos conectar con el exoesqueleto. Intente nuevamente acercándose al dispositivo y asegúrese de que el bluetooth esté activado.';

    return Scaffold(
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            colors: [Color(0xFF071120), Color(0xFF10293B)],
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
          ),
        ),
        child: SafeArea(
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 32),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                SizedBox(
                  width: 50,
                  height: 50,
                  child: Image.asset('assets/images/logo.png'),
                ),
                const SizedBox(height: 24),
                Container(
                  height: 1,
                  width: double.infinity,
                  color: Colors.white.withOpacity(0.18),
                ),
                const SizedBox(height: 36),
                Text(
                  title,
                  style: const TextStyle(
                    fontSize: 24,
                    fontWeight: FontWeight.w700,
                    color: Color(0xFFF4F9FF),
                  ),
                ),
                const SizedBox(height: 18),
                Text(
                  description,
                  style: const TextStyle(
                    fontSize: 16,
                    height: 1.6,
                    color: Color(0xFFD0E0EC),
                  ),
                ),
                const SizedBox(height: 16),
                if (scanState.error != null)
                  Container(
                    width: double.infinity,
                    padding: const EdgeInsets.symmetric(
                      horizontal: 12,
                      vertical: 10,
                    ),
                    margin: const EdgeInsets.only(bottom: 16),
                    decoration: BoxDecoration(
                      color: Colors.red.withOpacity(0.1),
                      borderRadius: BorderRadius.circular(12),
                      border: Border.all(color: Colors.redAccent),
                    ),
                    child: Text(
                      scanState.error!,
                      style: const TextStyle(color: Colors.redAccent),
                    ),
                  ),
                if (isScanning)
                  const LinearProgressIndicator(
                    minHeight: 3,
                    backgroundColor: Colors.transparent,
                    color: Color(0xFF7DAEB7),
                  ),
                const SizedBox(height: 24),
                Expanded(
                  child: hasDevices
                      ? ListView.separated(
                          itemCount: scanState.devices.length,
                          separatorBuilder: (_, __) =>
                              const SizedBox(height: 12),
                          itemBuilder: (context, index) {
                            final device = scanState.devices[index];
                            final name = device.name.isNotEmpty
                                ? device.name
                                : 'Dispositivo BLE';

                            return _DeviceTile(
                              device: device,
                              name: name,
                              isConnecting: isConnecting,
                              onConnect: () => _connectToDevice(device),
                            );
                          },
                        )
                      : Center(
                          child: Text(
                            isScanning
                                ? 'Buscando dispositivos...'
                                : 'No se detectaron dispositivos cercanos.',
                            style: TextStyle(
                              color: Colors.white.withOpacity(0.7),
                            ),
                          ),
                        ),
                ),
                const SizedBox(height: 24),
                SizedBox(
                  width: double.infinity,
                  height: 54,
                  child: ElevatedButton(
                    style: ElevatedButton.styleFrom(
                      backgroundColor: const Color(0xFF7DAEB7),
                      foregroundColor: const Color(0xFFF5FAFC),
                      elevation: 0,
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(16),
                      ),
                    ),
                    onPressed: isScanning ? null : _retryScan,
                    child: Text(
                      isScanning ? 'Buscando...' : 'Buscar nuevamente',
                      style: const TextStyle(
                        fontSize: 17,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _DeviceTile extends StatelessWidget {
  const _DeviceTile({
    required this.device,
    required this.name,
    required this.isConnecting,
    required this.onConnect,
  });

  final DiscoveredDevice device;
  final String name;
  final bool isConnecting;
  final VoidCallback onConnect;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
      decoration: BoxDecoration(
        color: Colors.white.withOpacity(0.04),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: Colors.white.withOpacity(0.1)),
      ),
      child: Row(
        children: [
          Container(
            width: 48,
            height: 48,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              gradient: LinearGradient(
                colors: [
                  const Color(0xFF8CCBD5).withOpacity(0.7),
                  const Color(0xFF295B62),
                ],
              ),
            ),
            child: const Icon(Icons.bluetooth, color: Colors.white),
          ),
          const SizedBox(width: 16),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  name,
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 16,
                    fontWeight: FontWeight.w600,
                  ),
                ),
                const SizedBox(height: 4),
                Text(
                  device.id,
                  style: TextStyle(
                    color: Colors.white.withOpacity(0.65),
                    fontSize: 12,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 12),
          ElevatedButton(
            onPressed: isConnecting ? null : onConnect,
            style: ElevatedButton.styleFrom(
              backgroundColor: const Color(0xFF3F797A),
              foregroundColor: Colors.white,
              elevation: 0,
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(8),
              ),
            ),
            child: Text(
              isConnecting ? 'Conectando...' : 'Conectar',
              style: const TextStyle(fontWeight: FontWeight.bold),
            ),
          ),
        ],
      ),
    );
  }
}
