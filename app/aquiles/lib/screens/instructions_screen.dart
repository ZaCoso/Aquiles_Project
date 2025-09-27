import 'package:flutter/material.dart';

class InstructionsScreen extends StatefulWidget {
  const InstructionsScreen({super.key});

  @override
  State<InstructionsScreen> createState() => _InstructionsScreenState();
}

class _InstructionsScreenState extends State<InstructionsScreen> {
  bool _showStepCard = false;

  void _handleStart() {
    if (_showStepCard) return;
    setState(() => _showStepCard = true);
  }

  void _handleNext() {
    Future.delayed(const Duration(milliseconds: 200), () {
      if (!mounted) return;
      Navigator.of(context).pushReplacementNamed('/instructions/step');
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            colors: [Color(0xFF081423), Color(0xFF10283A)],
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
                child: AbsorbPointer(
                  absorbing: _showStepCard,
                  child: AnimatedOpacity(
                    duration: const Duration(milliseconds: 280),
                    opacity: _showStepCard ? 0.45 : 1,
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        SizedBox(
                          width: 64,
                          height: 64,
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
                        const SizedBox(height: 48),
                        const Text(
                          'Instrucciones de uso',
                          style: TextStyle(
                            fontSize: 28,
                            fontWeight: FontWeight.bold,
                            color: Color(0xFFF8FBFF),
                          ),
                        ),
                        const SizedBox(height: 16),
                        const Text(
                          'Felicidades por completar el registro!',
                          style: TextStyle(
                            fontSize: 16,
                            height: 1.5,
                            color: Color(0xFFD6DEE7),
                          ),
                        ),
                        const SizedBox(height: 16),
                        const Text(
                          'A continuacion, en las instrucciones de uso, aprenderas como ajustar el producto para que se adapte correctamente a tu cuerpo.',
                          style: TextStyle(
                            fontSize: 16,
                            height: 1.6,
                            color: Color(0xFFD6DEE7),
                          ),
                        ),
                        const Spacer(),
                        AnimatedSwitcher(
                          duration: const Duration(milliseconds: 220),
                          child: _showStepCard
                              ? const SizedBox(height: 56)
                              : SizedBox(
                                  key: const ValueKey('start-button'),
                                  width: double.infinity,
                                  height: 56,
                                  child: ElevatedButton(
                                    style: ElevatedButton.styleFrom(
                                      backgroundColor: const Color(0xFF7DAEB7),
                                      foregroundColor: const Color(0xFFF5FAFC),
                                      elevation: 0,
                                      shape: RoundedRectangleBorder(
                                        borderRadius: BorderRadius.circular(18),
                                      ),
                                    ),
                                    onPressed: _handleStart,
                                    child: const Text(
                                      'Iniciar',
                                      style: TextStyle(
                                        fontSize: 18,
                                        fontWeight: FontWeight.w500,
                                      ),
                                    ),
                                  ),
                                ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ),
            AnimatedSwitcher(
              duration: const Duration(milliseconds: 420),
              switchInCurve: Curves.easeOutCubic,
              switchOutCurve: Curves.easeInCubic,
              transitionBuilder: (child, animation) {
                final fade = CurvedAnimation(
                  parent: animation,
                  curve: Curves.easeOutCubic,
                );
                final offsetAnimation = Tween<Offset>(
                  begin: const Offset(0, 0.45),
                  end: Offset.zero,
                ).animate(fade);
                return FadeTransition(
                  opacity: fade,
                  child: SlideTransition(
                    position: offsetAnimation,
                    child: child,
                  ),
                );
              },
              child: !_showStepCard
                  ? const SizedBox.shrink()
                  : Container(
                      key: const ValueKey('step-overlay'),
                      color: const Color(0xFF061320).withOpacity(0.55),
                      padding: const EdgeInsets.symmetric(
                        horizontal: 24,
                        vertical: 36,
                      ),
                      child: Align(
                        alignment: Alignment.bottomCenter,
                        child: AnimatedOpacity(
                          duration: const Duration(milliseconds: 240),
                          curve: Curves.easeOut,
                          opacity: _showStepCard ? 1 : 0,
                          child: _InstructionStepCard(onNext: _handleNext),
                        ),
                      ),
                    ),
            ),
          ],
        ),
      ),
    );
  }
}

class _InstructionStepCard extends StatelessWidget {
  const _InstructionStepCard({required this.onNext});

  final VoidCallback onNext;

  @override
  Widget build(BuildContext context) {
    return Container(
      constraints: const BoxConstraints(maxWidth: 380),
      padding: const EdgeInsets.all(24),
      decoration: BoxDecoration(
        color: const Color(0xFF112B3C).withOpacity(0.95),
        borderRadius: BorderRadius.circular(28),
        border: Border.all(
          color: const Color(0xFF8CCBD5).withOpacity(0.65),
          width: 1.2,
        ),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(0.45),
            offset: const Offset(0, 24),
            blurRadius: 38,
            spreadRadius: -18,
          ),
        ],
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          ClipRRect(
            borderRadius: BorderRadius.circular(18),
            child: Image.asset(
              'assets/images/exoesqueleto-1.png',
              height: 150,
              width: double.infinity,
              fit: BoxFit.cover,
              alignment: Alignment.center,
            ),
          ),
          const SizedBox(height: 24),
          const Text(
            '1. Se recomienda cargar completamente la bateria antes del primer uso.',
            style: TextStyle(
              color: Color(0xFFE4EDF5),
              fontSize: 16,
              height: 1.55,
            ),
          ),
          const SizedBox(height: 14),
          const Text(
            '2. Asegurese de que la bateria este insertada firmemente en sus respectivos conectores.',
            style: TextStyle(
              color: Color(0xFFE4EDF5),
              fontSize: 16,
              height: 1.55,
            ),
          ),
          const SizedBox(height: 28),
          SizedBox(
            width: double.infinity,
            height: 56,
            child: ElevatedButton(
              style: ElevatedButton.styleFrom(
                backgroundColor: const Color(0xFF7DAEB7),
                foregroundColor: const Color(0xFFF5FAFC),
                elevation: 0,
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(18),
                ),
              ),
              onPressed: onNext,
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
    );
  }
}
