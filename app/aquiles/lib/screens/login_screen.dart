import 'package:aquiles/providers/auth_provider.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'home_screen.dart';

class LoginScreen extends ConsumerStatefulWidget {
  const LoginScreen({super.key});

  @override
  ConsumerState<LoginScreen> createState() => _LoginScreenState();
}

class _LoginScreenState extends ConsumerState<LoginScreen>
    with SingleTickerProviderStateMixin {
  final _emailController = TextEditingController();
  final _passwordController = TextEditingController();
  bool _isLoading = false;
  bool _showSocial = false;
  late AnimationController _animationController;
  late Animation<double> _fadeAnimation;

  @override
  void initState() {
    super.initState();
    _animationController = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 900),
    );
    _fadeAnimation = Tween<double>(begin: 0.0, end: 1.0).animate(
      CurvedAnimation(
        parent: _animationController,
        curve: Curves.easeIn,
      ),
    );
    // Precache large images, then start fade animation to avoid first-frame jank.
    // Start immediately; also precache images without blocking first frame.
    _animationController.forward();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      // Fire-and-forget: don't block first frame on image decode
      precacheImage(
          const AssetImage('assets/images/exoesqueleto-1.png'), context);
      precacheImage(const AssetImage('assets/images/logo.png'), context);
      // If needed, ensure it's running (safe if already started)
      if (mounted &&
          !_animationController.isAnimating &&
          _animationController.value == 0.0) {
        _animationController.forward();
      }
      // Defer heavy SVG row until after first frame
      if (mounted) {
        setState(() => _showSocial = true);
      }
    });
  }

  @override
  void dispose() {
    _animationController.stop();
    _emailController.dispose();
    _passwordController.dispose();
    _animationController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Stack(children: [
        // Background gradient
        Container(
          decoration: BoxDecoration(
            gradient: LinearGradient(
              begin: Alignment.topCenter,
              end: Alignment(0.5, 1.8),
              colors: [
                const Color(0xFF0A1128),
                const Color(0xFF295B62).withOpacity(0.85),
              ],
            ),
          ),
        ),
        // Background exoskeleton image with low opacity
        Positioned.fill(
          child: IgnorePointer(
            child: Center(
              child: Opacity(
                opacity: 0.10,
                child: LayoutBuilder(
                  builder: (context, constraints) {
                    final targetWidthPx = (constraints.maxWidth *
                            MediaQuery.of(context).devicePixelRatio)
                        .round();
                    return Image.asset(
                      'assets/images/exoesqueleto-1.png',
                      fit: BoxFit.cover,
                      alignment: Alignment.center,
                      cacheWidth: targetWidthPx,
                      filterQuality: FilterQuality.low,
                    );
                  },
                ),
              ),
            ),
          ),
        ),
        SafeArea(
          child: Center(
            child: SingleChildScrollView(
              padding:
                  const EdgeInsets.symmetric(horizontal: 24.0, vertical: 14),
              child: FadeTransition(
                opacity: _fadeAnimation,
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    // Logo
                    Image.asset(
                      'assets/images/logo.png',
                      width: 72,
                      height: 72,
                    ),
                    const SizedBox(height: 28),

                    // Título

                    const SizedBox(height: 8),

                    const SizedBox(height: 24),

                    // Campos de inicio de sesión
                    Container(
                      color: Colors.transparent,
                      padding: const EdgeInsets.symmetric(horizontal: 0),
                      child: Column(
                        children: [
                          TextField(
                            controller: _emailController,
                            style: const TextStyle(color: Colors.white),
                            decoration: InputDecoration(
                              hintText: 'Correo electrónico',
                              hintStyle: TextStyle(
                                  color: Colors.white.withOpacity(0.85)),
                              filled: true,
                              fillColor:
                                  const Color(0xFF8CCBD5).withOpacity(0.18),
                              enabledBorder: OutlineInputBorder(
                                borderRadius: BorderRadius.circular(24),
                                borderSide: BorderSide(
                                  color:
                                      const Color(0xFF8CCBD5).withOpacity(0.35),
                                ),
                              ),
                              focusedBorder: OutlineInputBorder(
                                borderRadius: BorderRadius.circular(24),
                                borderSide: const BorderSide(
                                  color: Color(0xFF8CCBD5),
                                  width: 1.5,
                                ),
                              ),
                              prefixIcon: const Icon(
                                Icons.email_outlined,
                                color: Color(0xFF8CCBD5),
                              ),
                              contentPadding: const EdgeInsets.symmetric(
                                  vertical: 16, horizontal: 16),
                            ),
                          ),
                          const SizedBox(height: 20),
                          TextField(
                            controller: _passwordController,
                            obscureText: true,
                            enableSuggestions: false,
                            autocorrect: false,
                            style: const TextStyle(color: Colors.white),
                            decoration: InputDecoration(
                              hintText: 'Contraseña',
                              hintStyle: TextStyle(
                                  color: Colors.white.withOpacity(0.85)),
                              filled: true,
                              fillColor:
                                  const Color(0xFF8CCBD5).withOpacity(0.18),
                              enabledBorder: OutlineInputBorder(
                                borderRadius: BorderRadius.circular(24),
                                borderSide: BorderSide(
                                  color:
                                      const Color(0xFF8CCBD5).withOpacity(0.35),
                                ),
                              ),
                              focusedBorder: OutlineInputBorder(
                                borderRadius: BorderRadius.circular(24),
                                borderSide: const BorderSide(
                                  color: Color(0xFF8CCBD5),
                                  width: 1.5,
                                ),
                              ),
                              prefixIcon: const Icon(
                                Icons.lock_outline,
                                color: Color(0xFF8CCBD5),
                              ),
                              contentPadding: const EdgeInsets.symmetric(
                                  vertical: 16, horizontal: 16),
                            ),
                          ),
                          const SizedBox(height: 10),
                          Align(
                            alignment: Alignment.center,
                            child: TextButton(
                              onPressed: () {},
                              child: Text(
                                '¿Olvidaste tu contraseña?',
                                style: TextStyle(
                                  color: Colors.white.withOpacity(0.8),
                                  fontSize: 13,
                                ),
                              ),
                            ),
                          ),
                          const SizedBox(height: 10),
                          SizedBox(
                            width: double.infinity,
                            height: 54,
                            child: ElevatedButton(
                              onPressed: () async {
                                if (!mounted) return;
                                setState(() {
                                  _isLoading = true;
                                });
                                try {
                                  await ref.read(authServiceProvider).signIn(
                                        _emailController.text,
                                        _passwordController.text,
                                      );
                                  if (!mounted) return;
                                  setState(() {
                                    _isLoading = false;
                                  });
                                  Navigator.of(context)
                                      .pushNamed('/instructions');
                                } catch (e) {
                                  if (mounted) {
                                    setState(() {
                                      _isLoading = false;
                                    });
                                    ScaffoldMessenger.of(context).showSnackBar(
                                      SnackBar(
                                        content: Text(
                                            'Error al iniciar sesión: Vuelva a intentarlo'),
                                        backgroundColor: Colors.red,
                                      ),
                                    );
                                  }
                                  print('Login error: $e'); // For debugging
                                }
                              },
                              style: ElevatedButton.styleFrom(
                                backgroundColor:
                                    const Color(0xFF8CCBD5).withOpacity(0.85),
                                foregroundColor: const Color(0xFF0A1128),
                                shape: RoundedRectangleBorder(
                                  borderRadius: BorderRadius.circular(24),
                                ),
                                elevation: 0,
                              ),
                              child: _isLoading
                                  ? const SizedBox(
                                      width: 24,
                                      height: 24,
                                      child: CircularProgressIndicator(
                                        color: Color(0xFF0A1128),
                                        strokeWidth: 2,
                                      ),
                                    )
                                  : const Text(
                                      'Iniciar sesión',
                                      style: TextStyle(
                                        color: Colors.white,
                                        fontSize: 18,
                                        fontWeight: FontWeight.bold,
                                      ),
                                    ),
                            ),
                          ),
                          const SizedBox(height: 18),
                          // Divider "o continuar con"
                          Row(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              Text(
                                '¿No tienes una cuenta? ',
                                style: TextStyle(
                                  color: Colors.white.withValues(alpha: .7),
                                ),
                              ),
                              TextButton(
                                onPressed: () {
                                  Navigator.of(context).pushNamed('/register');
                                },
                                child: const Text(
                                  'Regístrate',
                                  style: TextStyle(
                                    color: Color(0xFF8CCBD5),
                                    fontWeight: FontWeight.bold,
                                  ),
                                ),
                              ),
                            ],
                          ),
                          const SizedBox(height: 18),

                          Row(
                            children: [
                              Expanded(
                                child: Divider(
                                  color: Colors.white.withOpacity(0.25),
                                  thickness: 1,
                                ),
                              ),
                              Padding(
                                padding:
                                    const EdgeInsets.symmetric(horizontal: 8.0),
                                child: Text(
                                  'Inicia sesión con',
                                  style: TextStyle(
                                    color: Colors.white.withOpacity(0.75),
                                    fontSize: 12,
                                  ),
                                ),
                              ),
                              Expanded(
                                child: Divider(
                                  color: Colors.white.withOpacity(0.25),
                                  thickness: 1,
                                ),
                              ),
                            ],
                          ),
                          const SizedBox(height: 16),
                          // Fila de iconos sociales (defer to post-frame)
                          if (_showSocial)
                            Row(
                              mainAxisAlignment: MainAxisAlignment.center,
                              children: [
                                IconButton(
                                  onPressed: () {
                                    () async {
                                      if (!mounted) return;
                                      setState(() => _isLoading = true);
                                      try {
                                        final user = await ref
                                            .read(authServiceProvider)
                                            .signInWithGoogle();
                                        if (mounted) {
                                          setState(() => _isLoading = false);
                                        }
                                      } catch (e) {
                                        if (mounted) {
                                          setState(() => _isLoading = false);
                                          ScaffoldMessenger.of(context)
                                              .showSnackBar(
                                            const SnackBar(
                                              content: Text(
                                                  'No se pudo iniciar con Google'),
                                              backgroundColor: Colors.red,
                                            ),
                                          );
                                        }
                                        print('Google login error: $e');
                                      }
                                    }();
                                  },
                                  padding: EdgeInsets.zero,
                                  iconSize: 48,
                                  icon: Container(
                                    width: 48,
                                    height: 48,
                                    decoration: BoxDecoration(
                                      color: Colors.white.withOpacity(0.06),
                                      shape: BoxShape.circle,
                                      border: Border.all(
                                        color:
                                            Color(0xFF8CCBD5).withOpacity(0.3),
                                      ),
                                    ),
                                    alignment: Alignment.center,
                                    child: SvgPicture.asset(
                                      'assets/images/google_icon.svg',
                                      width: 24,
                                      height: 24,
                                      colorFilter: const ColorFilter.mode(
                                        Color(0xFF8CCBD5),
                                        BlendMode.srcIn,
                                      ),
                                    ),
                                  ),
                                ),
                              ],
                            ),
                          const SizedBox(height: 48),
                        ],
                      ),
                    ),
                    const SizedBox(height: 24),

                    // Registro
                  ],
                ),
              ),
            ),
          ),
        ),
      ]),
    );
  }
}
