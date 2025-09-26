import 'dart:io';

import 'package:aquiles/providers/ble_connection_provider.dart';
import 'package:aquiles/providers/ble_provider.dart';
import 'package:aquiles/services/ble_services.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

class UnconnectedStatusTab extends ConsumerStatefulWidget {
  const UnconnectedStatusTab({Key? key}) : super(key: key);

  @override
  ConsumerState<UnconnectedStatusTab> createState() => _UnconnectedStatusTabState();
}

class _UnconnectedStatusTabState extends ConsumerState<UnconnectedStatusTab> {
  late BleScanNotifier _scanner;
  @override
  void initState() {
    super.initState();
    // Prepare scanner immediately and start continuous scanning
    _scanner = ref.read(bleScanProvider.notifier);
    _scanner.startScan();
  }

  @override
  void dispose() {
    // Stop scanning when leaving this tab
    try {
      _scanner.stopScan();
    } catch (_) {}
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final ref = this.ref;
    final bleState = ref.watch(bleConnectionProvider);
    final ble = ref.read(bleConnectionProvider.notifier);

    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            decoration: BoxDecoration(
              gradient: LinearGradient(
                begin: Alignment.topLeft,
                end: Alignment.bottomRight,
                colors: [
                  const Color(0xFF0A1128),
                  const Color(0xFF295B62),
                ],
              ),
              borderRadius: BorderRadius.circular(16),
              border: Border.all(
                color: const Color(0xFF3F797A).withOpacity(0.5),
              ),
              boxShadow: [
                BoxShadow(
                  color: const Color(0xFF8CCBD5).withOpacity(0.15),
                  blurRadius: 15,
                  spreadRadius: 0,
                ),
              ],
            ),
            child: Padding(
              padding: const EdgeInsets.all(20),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      Text(
                        'Exoesqueleto X1',
                        style: Theme.of(context).textTheme.titleLarge,
                      ),
                    ],
                  ),
                  const SizedBox(height: 20),
                  Center(
                    child: Stack(
                      alignment: Alignment.center,
                      children: [
                        Container(
                          width: 200,
                          height: 200,
                          decoration: BoxDecoration(
                            shape: BoxShape.circle,
                            gradient: RadialGradient(
                              colors: [
                                const Color(0xFF8CCBD5).withOpacity(0.1),
                                Colors.transparent,
                              ],
                              radius: 0.8,
                            ),
                          ),
                        ),
                        Image.asset(
                          'assets/images/exoesqueleto-1.png',
                          width: 240,
                          height: 240,
                        ),
                      ],
                    ),
                  ),
                  const SizedBox(height: 8),
                  Consumer(
                    builder: (context, ref, _) {
                      final scanState = ref.watch(bleScanProvider);
                      final scanner = _scanner;
                      return Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          if (scanState.devices.isNotEmpty) ...[
                            const SizedBox(height: 8),
                            Text(
                              'Dispositivos encontrados:',
                              style: Theme.of(context).textTheme.titleMedium,
                            ),
                            const SizedBox(height: 8),
                            Column(
                              children: scanState.devices
                                  .map(
                                    (d) => Container(
                                      width: double.infinity,
                                      margin: const EdgeInsets.only(bottom: 8),
                                      padding: const EdgeInsets.all(12),
                                      decoration: BoxDecoration(
                                        color: const Color(0xFF0A1128)
                                            .withOpacity(0.6),
                                        borderRadius: BorderRadius.circular(8),
                                        border: Border.all(
                                          color: const Color(0xFF3F797A)
                                              .withOpacity(0.3),
                                        ),
                                      ),
                                      child: Row(
                                        children: [
                                          Expanded(
                                            child: Column(
                                              crossAxisAlignment:
                                                  CrossAxisAlignment.start,
                                              children: [
                                                Text(
                                                  d.name.isNotEmpty
                                                      ? d.name
                                                      : 'Dispositivo BLE',
                                                  style: const TextStyle(
                                                    color: Color(0xFFFFFFFA),
                                                    fontWeight: FontWeight.bold,
                                                  ),
                                                ),
                                                const SizedBox(height: 4),
                                                Text(
                                                  d.id,
                                                  style: TextStyle(
                                                    color: Colors.white
                                                        .withOpacity(0.7),
                                                    fontSize: 12,
                                                  ),
                                                ),
                                              ],
                                            ),
                                          ),
                                          const SizedBox(width: 8),
                                          ElevatedButton(
                                            onPressed: bleState ==
                                                    BleConnectionState.connecting
                                                ? null
                                                : () async {
                                                    await scanner.stopScan();
                                                    final success =
                                                        await ble.connectToId(
                                                            d.id);
                                                    if (success) {
                                                      await ble
                                                          .sendCommand('1');
                                                    } else {
                                                      // Remove unavailable device from list
                                                      print('[BLE] Connection failed/unavailable for ' + d.id + ' (' + (d.name.isNotEmpty ? d.name : 'Dispositivo BLE') + '), removing from list');
                                                      ref
                                                          .read(bleScanProvider
                                                              .notifier)
                                                          .removeDevice(d.id);
                                                      ScaffoldMessenger.of(
                                                              context)
                                                          .showSnackBar(
                                                        const SnackBar(
                                                          content: Text(
                                                              'Dispositivo no disponible. Eliminado de la lista.'),
                                                          backgroundColor:
                                                              Colors.red,
                                                        ),
                                                      );
                                                    }
                                                  },
                                            style: ElevatedButton.styleFrom(
                                              backgroundColor:
                                                  const Color(0xFF3F797A),
                                              foregroundColor: Colors.white,
                                              elevation: 0,
                                              padding:
                                                  const EdgeInsets.symmetric(
                                                      horizontal: 16,
                                                      vertical: 10),
                                              shape: RoundedRectangleBorder(
                                                borderRadius:
                                                    BorderRadius.circular(8),
                                              ),
                                            ),
                                            child: Text(
                                              bleState ==
                                                      BleConnectionState
                                                          .connecting
                                                  ? 'Conectando...'
                                                  : 'Conectar',
                                              style: const TextStyle(
                                                fontWeight: FontWeight.bold,
                                              ),
                                            ),
                                          ),
                                        ],
                                      ),
                                    ),
                                  )
                                  .toList(),
                            ),
                          ],
                          if (scanState.devices.isEmpty)
                            Text(
                              'No se encontraron dispositivos aún.',
                              style: TextStyle(
                                color: Colors.white.withOpacity(0.7),
                                fontSize: 12,
                              ),
                            ),
                        ],
                      );
                    },
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 16),
        ],
      ),
    );
  }
}
