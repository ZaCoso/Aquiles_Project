import 'package:flutter/material.dart';

import '../widgets/instruction_step_layout.dart';

const _defaultSteps = [
  InstructionStepData(
    title: 'Ajuste de cinturon lumbar',
    stepNumber: 1,
    totalSteps: 4,
    imageAssetPath: 'assets/images/exoesqueleto-1.png',
    instructions: [
      'Mantenga presionado el botón situado en la parte superior del módulo de la espalda baja.',
      'Luego tire del cinturón y cuando coloque el mismo en la medida deseada suelte el pulsador.',
    ],
  ),
  InstructionStepData(
    title: 'Ajuste faja de cadera',
    stepNumber: 2,
    totalSteps: 4,
    imageAssetPath: 'assets/images/exoesqueleto-1.png',
    instructions: [
      'Tome ambos extremos de la faja.',
      'Superponga uno sobre el otro y ajústelos a la medida deseada.',
    ],
  ),
];

class StepDetailScreen extends StatefulWidget {
  const StepDetailScreen({
    super.key,
    this.step,
    this.steps,
  });

  final InstructionStepData? step;
  final List<InstructionStepData>? steps;

  @override
  State<StepDetailScreen> createState() => _StepDetailScreenState();
}

class _StepDetailScreenState extends State<StepDetailScreen> {
  late List<InstructionStepData> _steps;
  int _currentIndex = 0;
  bool _initialized = false;

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (_initialized) return;
    _initialized = true;

    final args = ModalRoute.of(context)?.settings.arguments;

    if (args is List<InstructionStepData> && args.isNotEmpty) {
      _steps = args;
    } else if (args is InstructionStepData) {
      _steps = [args];
    } else if (widget.steps != null && widget.steps!.isNotEmpty) {
      _steps = widget.steps!;
    } else if (widget.step != null) {
      _steps = [widget.step!];
    } else {
      _steps = _defaultSteps;
    }
  }

  @override
  Widget build(BuildContext context) {
    final currentStep = _steps[_currentIndex];
    final isLastStep = _currentIndex == _steps.length - 1;

    return InstructionStepLayout(
      step: currentStep,
      onPrimaryAction: () {
        if (!isLastStep) {
          setState(() => _currentIndex++);
          return;
        }
        Navigator.of(context).popUntil((route) => route.isFirst);
      },
      primaryActionLabel: isLastStep ? 'Finalizar' : 'Siguiente',
    );
  }
}
