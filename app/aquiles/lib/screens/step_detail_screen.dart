import 'package:flutter/material.dart';

class StepDetailScreen extends StatelessWidget {
  const StepDetailScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            colors: [Color(0xFF071120), Color(0xFF10293B)],
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
          ),
        ),
        child: Stack(
          children: [
            Positioned.fill(
              child: DecoratedBox(
                decoration: BoxDecoration(
                  gradient: RadialGradient(
                    center: const Alignment(0, -0.1),
                    radius: 1.2,
                    colors: [
                      const Color(0xFF1A3B4A).withOpacity(0.35),
                      Colors.transparent,
                    ],
                  ),
                ),
              ),
            ),
            SafeArea(
              child: Padding(
                padding:
                    const EdgeInsets.symmetric(horizontal: 32, vertical: 24),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    SizedBox(
                      width: 52,
                      height: 52,
                      child: Image.asset(
                        'assets/images/logo.png',
                        fit: BoxFit.contain,
                      ),
                    ),
                    const SizedBox(height: 24),
                    Container(
                      height: 1,
                      width: double.infinity,
                      color: Colors.white.withOpacity(0.15),
                    ),
                    const SizedBox(height: 36),
                    Expanded(
                      child: Align(
                        alignment: Alignment.topCenter,
                        child: ConstrainedBox(
                          constraints: const BoxConstraints(maxWidth: 420),
                          child: SingleChildScrollView(
                            physics: const BouncingScrollPhysics(),
                            child: Column(
                              children: [
                                Container(
                                  decoration: BoxDecoration(
                                    borderRadius: BorderRadius.circular(28),
                                    color: const Color(0xFF123347).withOpacity(0.97),
                                    border: Border.all(
                                      color: const Color(0xFF8FC3CE),
                                      width: 1.1,
                                    ),
                                    boxShadow: const [
                                      BoxShadow(
                                        color: Color(0x330F171F),
                                        offset: Offset(0, 24),
                                        blurRadius: 38,
                                        spreadRadius: -18,
                                      ),
                                    ],
                                  ),
                                  padding:
                                      const EdgeInsets.symmetric(horizontal: 26, vertical: 14),
                                  child: Column(
                                    crossAxisAlignment: CrossAxisAlignment.start,
                                    children: [
                                      ClipRRect(
                                        borderRadius: BorderRadius.circular(22),
                                        child: Image.asset(
                                          'assets/images/exoesqueleto-1.png',
                                          height: 214,
                                          width: double.infinity,
                                          fit: BoxFit.cover,
                                          alignment: Alignment.topCenter,
                                        ),
                                      ),
                                      const SizedBox(height: 24),
                                      const Text(
                                        'Ajuste de cinturon lumbar',
                                        style: TextStyle(
                                          fontSize: 23,
                                          fontWeight: FontWeight.w700,
                                          color: Color(0xFFF4F9FF),
                                        ),
                                      ),
                                      const SizedBox(height: 6),
                                      const Text(
                                        '(1/4)',
                                        style: TextStyle(
                                          fontSize: 15,
                                          color: Color(0xFFA9BECD),
                                        ),
                                      ),
                                      const SizedBox(height: 20),
                                      const Text(
                                        '1. Mantenga presionado el botón situado en la parte superior del módulo de la espalda baja.',
                                        style: TextStyle(
                                          color: Color(0xFFD4E3EF),
                                          fontSize: 15.5,
                                          height: 1.55,
                                        ),
                                      ),
                                      const SizedBox(height: 14),
                                      const Text(
                                        '2. Luego tire del cinturón y cuando coloque el mismo en la medida deseada suelte el pulsador.',
                                        style: TextStyle(
                                          color: Color(0xFFD4E3EF),
                                          fontSize: 15.5,
                                          height: 1.55,
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                                const SizedBox(height: 32),
                              ],
                            ),
                          ),
                        ),
                      ),
                    ),
                    SizedBox(
                      width: double.infinity,
                      height: 56,
                      child: ElevatedButton(
                        style: ElevatedButton.styleFrom(
                          backgroundColor: const Color(0xFF78AEB7),
                          foregroundColor: const Color(0xFFF5FAFC),
                          elevation: 0,
                          shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(18),
                          ),
                        ),
                        onPressed: () {
                          Navigator.of(context)
                              .popUntil((route) => route.isFirst);
                        },
                        child: const Text(
                          'Siguiente',
                          style: TextStyle(
                            fontSize: 18,
                            fontWeight: FontWeight.w500,
                          ),
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
