import 'package:aquiles/providers/auth_provider.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_svg/flutter_svg.dart';

class RegisterScreen extends ConsumerStatefulWidget {
  const RegisterScreen({super.key});

  @override
  ConsumerState<RegisterScreen> createState() => _RegisterScreenState();
}

class _RegisterScreenState extends ConsumerState<RegisterScreen>
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
    _animationController.forward();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      precacheImage(
          const AssetImage('assets/images/exoesqueleto-1.png'), context);
      precacheImage(const AssetImage('assets/images/logo.png'), context);
      if (mounted) {
        if (!_animationController.isAnimating &&
            _animationController.value == 0.0) {
          _animationController.forward();
        }
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
      body: Stack(
        children: [
          Container(
            decoration: BoxDecoration(
              gradient: LinearGradient(
                begin: Alignment.topCenter,
                end: const Alignment(0.5, 1.8),
                colors: [
                  const Color(0xFF0A1128),
                  const Color(0xFF295B62).withOpacity(0.85),
                ],
              ),
            ),
          ),
          Positioned.fill(
            child: IgnorePointer(
              child: Center(
                child: Opacity(
                  opacity: 0.1,
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
                      Image.asset(
                        'assets/images/logo.png',
                        width: 72,
                        height: 72,
                      ),
                      const SizedBox(height: 28),
                      Text(
                        'Crea tu cuenta',
                        style: Theme.of(context)
                            .textTheme
                            .titleLarge
                            ?.copyWith(fontSize: 24),
                      ),
                      const SizedBox(height: 8),
                      Text(
                        'Ingresa tu correo y una contraseña segura',
                        textAlign: TextAlign.center,
                        style: Theme.of(context)
                            .textTheme
                            .bodyMedium
                            ?.copyWith(color: Colors.white.withOpacity(0.7)),
                      ),
                      const SizedBox(height: 24),
                      Container(
                        color: Colors.transparent,
                        child: Column(
                          children: [
                            TextField(
                              controller: _emailController,
                              keyboardType: TextInputType.emailAddress,
                              style: const TextStyle(color: Colors.white),
                              decoration: InputDecoration(
                                hintText: 'Correo electrónico',
                                hintStyle: TextStyle(
                                  color: Colors.white.withOpacity(0.85),
                                ),
                                filled: true,
                                fillColor:
                                    const Color(0xFF8CCBD5).withOpacity(0.18),
                                enabledBorder: OutlineInputBorder(
                                  borderRadius: BorderRadius.circular(24),
                                  borderSide: BorderSide(
                                    color: const Color(0xFF8CCBD5)
                                        .withOpacity(0.35),
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
                                  vertical: 16,
                                  horizontal: 16,
                                ),
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
                                  color: Colors.white.withOpacity(0.85),
                                ),
                                filled: true,
                                fillColor:
                                    const Color(0xFF8CCBD5).withOpacity(0.18),
                                enabledBorder: OutlineInputBorder(
                                  borderRadius: BorderRadius.circular(24),
                                  borderSide: BorderSide(
                                    color: const Color(0xFF8CCBD5)
                                        .withOpacity(0.35),
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
                                  vertical: 16,
                                  horizontal: 16,
                                ),
                              ),
                            ),
                            const SizedBox(height: 24),
                            SizedBox(
                              width: double.infinity,
                              height: 54,
                              child: ElevatedButton(
                                onPressed: _isLoading ? null : _onRegister,
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
                                        'Crear cuenta',
                                        style: TextStyle(
                                          color: Colors.white,
                                          fontSize: 18,
                                          fontWeight: FontWeight.bold,
                                        ),
                                      ),
                              ),
                            ),
                            const SizedBox(height: 18),
                            Row(
                              mainAxisAlignment: MainAxisAlignment.center,
                              children: [
                                Text(
                                  '¿Ya tienes una cuenta? ',
                                  style: TextStyle(
                                    color: Colors.white.withOpacity(0.7),
                                  ),
                                ),
                                TextButton(
                                  onPressed: () {
                                    Navigator.of(context)
                                        .pushReplacementNamed('/login');
                                  },
                                  child: const Text(
                                    'Inicia sesión',
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
                                  padding: const EdgeInsets.symmetric(
                                      horizontal: 8.0),
                                  child: Text(
                                    'O regístrate con',
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
                            if (_showSocial)
                              Row(
                                mainAxisAlignment: MainAxisAlignment.center,
                                children: [
                                  IconButton(
                                    onPressed: _isLoading
                                        ? null
                                        : () {
                                            _handleGoogleAuth(context);
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
                                          color: const Color(0xFF8CCBD5)
                                              .withOpacity(0.3),
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
                    ],
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Future<void> _onRegister() async {
    setState(() => _isLoading = true);
    try {
      await ref.read(authServiceProvider).register(
            _emailController.text.trim(),
            _passwordController.text,
          );
      if (!mounted) return;
      setState(() => _isLoading = false);
      Navigator.of(context).pushNamed('/instructions');
    } catch (e) {
      if (!mounted) return;
      setState(() => _isLoading = false);
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content:
              Text('Error al crear la cuenta. Verifica tus datos e inténtalo.'),
          backgroundColor: Colors.red,
        ),
      );
      print('Register error: $e');
    }
  }

  Future<void> _handleGoogleAuth(BuildContext context) async {
    if (!mounted) return;
    setState(() => _isLoading = true);
    try {
      await ref.read(authServiceProvider).signInWithGoogle();
      if (!mounted) return;
      setState(() => _isLoading = false);
      Navigator.of(context).pushNamed('/instructions');
    } catch (e) {
      if (!mounted) return;
      setState(() => _isLoading = false);
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('No se pudo registrar con Google'),
          backgroundColor: Colors.red,
        ),
      );
      print('Google register error: $e');
    }
  }
}
