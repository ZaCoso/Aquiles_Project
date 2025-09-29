import 'package:flutter/material.dart';

/// Immutable data describing a single instruction step.
class InstructionStepData {
  const InstructionStepData({
    required this.title,
    required this.stepNumber,
    required this.totalSteps,
    required this.imageAssetPath,
    required this.instructions,
  });

  final String title;
  final int stepNumber;
  final int totalSteps;
  final String imageAssetPath;
  final List<String> instructions;
}

/// Reusable layout used to display detailed instructions for a single step.
class InstructionStepLayout extends StatelessWidget {
  const InstructionStepLayout({
    super.key,
    required this.step,
    required this.onPrimaryAction,
    this.primaryActionLabel = 'Siguiente',
  });

  final InstructionStepData step;
  final VoidCallback onPrimaryAction;
  final String primaryActionLabel;

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
                                    color: const Color(0xFF123347)
                                        .withOpacity(0.97),
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
                                  padding: const EdgeInsets.symmetric(
                                    horizontal: 26,
                                    vertical: 14,
                                  ),
                                  child: Column(
                                    crossAxisAlignment:
                                        CrossAxisAlignment.start,
                                    children: [
                                      ClipRRect(
                                        borderRadius: BorderRadius.circular(22),
                                        child: Image.asset(
                                          step.imageAssetPath,
                                          height: 214,
                                          width: double.infinity,
                                          fit: BoxFit.cover,
                                          alignment: Alignment.topCenter,
                                        ),
                                      ),
                                      const SizedBox(height: 24),
                                      Text(
                                        step.title,
                                        style: const TextStyle(
                                          fontSize: 23,
                                          fontWeight: FontWeight.w700,
                                          color: Color(0xFFF4F9FF),
                                        ),
                                      ),
                                      const SizedBox(height: 6),
                                      Text(
                                        '(${step.stepNumber}/${step.totalSteps})',
                                        style: const TextStyle(
                                          fontSize: 15,
                                          color: Color(0xFFA9BECD),
                                        ),
                                      ),
                                      const SizedBox(height: 20),
                                      ..._buildInstructionTexts(
                                          step.instructions),
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
                        onPressed: onPrimaryAction,
                        child: Text(
                          primaryActionLabel,
                          style: const TextStyle(
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

  List<Widget> _buildInstructionTexts(List<String> instructions) {
    if (instructions.isEmpty) {
      return const [];
    }

    final children = <Widget>[];
    for (var index = 0; index < instructions.length; index++) {
      final instructionText = instructions[index];
      children.add(
        Text(
          '${index + 1}. $instructionText',
          style: const TextStyle(
            color: Color(0xFFD4E3EF),
            fontSize: 15.5,
            height: 1.55,
          ),
        ),
      );
      if (index != instructions.length - 1) {
        children.add(const SizedBox(height: 14));
      }
    }
    return children;
  }
}
