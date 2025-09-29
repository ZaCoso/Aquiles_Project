import 'package:flutter/material.dart';

import '../widgets/instruction_step_layout.dart';

const _defaultStep = InstructionStepData(
  title: 'Ajuste de cinturon lumbar',
  stepNumber: 1,
  totalSteps: 4,
  imageAssetPath: 'assets/images/exoesqueleto-1.png',
  instructions: [
    'Mantenga presionado el botón situado en la parte superior del módulo de la espalda baja.',
    'Luego tire del cinturón y cuando coloque el mismo en la medida deseada suelte el pulsador.',
  ],
);

class StepDetailScreen extends StatelessWidget {
  const StepDetailScreen({super.key, this.step});

  final InstructionStepData? step;

  @override
  Widget build(BuildContext context) {
    final routeArgs = ModalRoute.of(context)?.settings.arguments;
    InstructionStepData stepData;

    if (routeArgs is InstructionStepData) {
      stepData = routeArgs;
    } else {
      stepData = step ?? _defaultStep;
    }

    return InstructionStepLayout(
      step: stepData,
      onPrimaryAction: () {
        Navigator.of(context).popUntil((route) => route.isFirst);
      },
    );
  }
}
