import 'dart:async';

import 'package:flutter/material.dart';

class DeviceScanScreen extends StatefulWidget {
  const DeviceScanScreen({super.key});

  @override
  State<DeviceScanScreen> createState() => _DeviceScanScreenState();
}

class _DeviceScanScreenState extends State<DeviceScanScreen> {
  static const int _scanDurationSeconds = 30;
  late int _secondsLeft;
  Timer? _timer;
  bool _scanCompleted = false;

  @override
  void initState() {
    super.initState();
    _secondsLeft = _scanDurationSeconds;
    _startTimer();
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  void _startTimer() {
    _timer?.cancel();
    _timer = Timer.periodic(const Duration(seconds: 1), (timer) {
      if (_secondsLeft <= 1) {
        timer.cancel();
        setState(() {
          _secondsLeft = 0;
          _scanCompleted = true;
        });
      } else {
        setState(() {
          _secondsLeft -= 1;
        });
      }
    });
  }

  void _handleRetry() {
    setState(() {
      _scanCompleted = false;
      _secondsLeft = _scanDurationSeconds;
    });
    _startTimer();
  }

  @override
  Widget build(BuildContext context) {
    final title = _scanCompleted
        ? 'No se encontraron dispositivos'
        : 'Buscando dispositivos cercanos (${_secondsLeft}s)';
    final description = _scanCompleted
        ? 'No pudimos conectar con el exoesqueleto. Intente nuevamente acercándose al dispositivo y asegúrese de que el bluetooth esté activado.'
        : 'Manténgase cerca de los dispositivos y asegúrese de que esté colocado, encendido el exoesqueleto y el bluetooth.';

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
                const Spacer(),
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
                    onPressed: _scanCompleted ? _handleRetry : null,
                    child: Text(
                      _scanCompleted ? 'Buscar nuevamente' : 'Buscando...',
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
