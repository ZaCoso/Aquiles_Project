import 'package:flutter/material.dart';
import 'package:video_player/video_player.dart';

class StepDetailScreen extends StatefulWidget {
  const StepDetailScreen({super.key});

  @override
  State<StepDetailScreen> createState() => _StepDetailScreenState();
}

class _StepDetailScreenState extends State<StepDetailScreen> {
  int _currentStepIndex = 0;

  static final _steps = [
    StepContent(
      title: 'Ajuste de cinturón lumbar',
      subtitle: '(1/4)',
      instructions: const [
        '1. Mantenga presionado el botón situado en la parte superior del módulo de la espalda baja.',
        '2. Luego tire del cinturón y cuando coloque el mismo en la medida deseada suelte el pulsador.',
      ],
      videoAsset: 'assets/videos/vid1.mp4',
    ),
    StepContent(
      title: 'Ajuste faja de cadera',
      subtitle: '(2/4)',
      instructions: const [
        '1. Tome ambos extremos de la faja.',
        '2. Superponga uno sobre el otro y ajústelos a la medida deseada.',
      ],
      videoAsset: 'assets/videos/vid1.mp4',
    ),
    StepContent(
      title: 'Ajuste extensiones de pierna',
      subtitle: '(3/4)',
      instructions: const [
        '1. Mantenga ambas piernas rectas y coloque las correas delanteras de pierna en la parte frontal de los muslos.',
        '2. Ajuste tirando de la cinta hasta lograr el calce deseado.',
      ],
      videoAsset: 'assets/videos/vid1.mp4',
    ),
    StepContent(
      title: 'Encender',
      subtitle: '(4/4)',
      instructions: const [
        '1. Presione el pulsador situado en el módulo de la espalda baja.',
      ],
      videoAsset: 'assets/videos/vid1.mp4',
    ),
  ];

  void _handleNext(BuildContext context) {
    if (_currentStepIndex >= _steps.length - 1) {
      Navigator.of(context).pushReplacementNamed('/device-scan');
      return;
    }
    setState(() {
      _currentStepIndex += 1;
    });
  }

  @override
  Widget build(BuildContext context) {
    final step = _steps[_currentStepIndex];

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
                      width: 50,
                      height: 50,
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
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                _StepVideoPlayer(
                                  key: ValueKey(step.videoAsset),
                                  assetPath: step.videoAsset,
                                ),
                                const SizedBox(height: 26),
                                Text(
                                  step.title,
                                  style: const TextStyle(
                                    fontSize: 24,
                                    fontWeight: FontWeight.w700,
                                    color: Color(0xFFF4F9FF),
                                    height: 1.2,
                                  ),
                                ),
                                const SizedBox(height: 6),
                                Text(
                                  step.subtitle,
                                  style: const TextStyle(
                                    fontSize: 15,
                                    fontWeight: FontWeight.w600,
                                    color: Color(0xFFA9BECD),
                                  ),
                                ),
                                const SizedBox(height: 18),
                                ...List.generate(step.instructions.length, (index) {
                                  final instruction = step.instructions[index];
                                  final isLast = index == step.instructions.length - 1;
                                  return Padding(
                                    padding: EdgeInsets.only(bottom: isLast ? 0 : 14),
                                    child: Text(
                                      instruction,
                                      style: const TextStyle(
                                        color: Color(0xFFD4E3EF),
                                        fontSize: 16,
                                        height: 1.55,
                                      ),
                                    ),
                                  );
                                }),
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
                          backgroundColor: const Color(0xFF7DAEB7),
                          foregroundColor: const Color(0xFFF5FAFC),
                          elevation: 0,
                          shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(18),
                          ),
                        ),
                        onPressed: () => _handleNext(context),
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

class StepContent {
  const StepContent({
    required this.title,
    required this.subtitle,
    required this.instructions,
    required this.videoAsset,
  });

  final String title;
  final String subtitle;
  final List<String> instructions;
  final String videoAsset;
}

class _StepVideoPlayer extends StatefulWidget {
  const _StepVideoPlayer({required this.assetPath, super.key});

  final String assetPath;

  @override
  State<_StepVideoPlayer> createState() => _StepVideoPlayerState();
}

class _StepVideoPlayerState extends State<_StepVideoPlayer>
    with AutomaticKeepAliveClientMixin {
  late final VideoPlayerController _controller;
  late final Future<void> _initializeVideo;

  @override
  void initState() {
    super.initState();
    _controller = VideoPlayerController.asset(widget.assetPath);
    _initializeVideo = _controller.initialize().then((_) async {
      if (!mounted) return;
      await _controller.setVolume(0);
      await _controller.setLooping(true);
      await _controller.play();
      setState(() {});
    });
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    super.build(context);
    return ClipRRect(
      borderRadius: BorderRadius.circular(26),
      child: SizedBox(
        width: double.infinity,
        height: 214,
        child: FutureBuilder<void>(
          future: _initializeVideo,
          builder: (context, snapshot) {
            if (snapshot.connectionState != ConnectionState.done) {
              return Container(
                color: const Color(0xFF0A1A2A),
                alignment: Alignment.center,
                child: const SizedBox(
                  height: 30,
                  width: 30,
                  child: CircularProgressIndicator(
                    strokeWidth: 2.5,
                    valueColor:
                        AlwaysStoppedAnimation<Color>(Color(0xFF8CCBD5)),
                  ),
                ),
              );
            }

            if (snapshot.hasError) {
              return Container(
                color: const Color(0xFF0A1A2A),
                alignment: Alignment.center,
                child: const Icon(Icons.error_outline, color: Colors.white70),
              );
            }

            final videoSize = _controller.value.size;
            final width = videoSize.width > 0 ? videoSize.width : 1.0;
            final height = videoSize.height > 0 ? videoSize.height : 1.0;

            return Stack(
              fit: StackFit.expand,
              children: [
                Container(
                  color: Colors.black,
                  child: FittedBox(
                    fit: BoxFit.cover,
                    alignment: Alignment.center,
                    child: SizedBox(
                      width: width,
                      height: height,
                      child: VideoPlayer(_controller),
                    ),
                  ),
                ),
                Positioned(
                  left: 16,
                  bottom: 16,
                  child: Container(
                    width: 36,
                    height: 36,
                    decoration: BoxDecoration(
                      color: Colors.white.withOpacity(0.85),
                      shape: BoxShape.circle,
                    ),
                    child: const Icon(
                      Icons.play_arrow_rounded,
                      color: Color(0xFF112B3C),
                      size: 26,
                    ),
                  ),
                ),
              ],
            );
          },
        ),
      ),
    );
  }

  @override
  bool get wantKeepAlive => true;
}
