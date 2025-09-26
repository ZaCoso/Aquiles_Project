import 'package:aquiles/providers/auth_provider.dart';
import 'package:aquiles/screens/home_screen.dart';
import 'package:aquiles/screens/login_screen.dart';
import 'package:flutter/material.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

class AuthChecker extends ConsumerWidget {
  const AuthChecker({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final authState = ref.watch(authStateProvider);
    print('AuthChecker');
    return authState.when(
      data: (user) {
        if (user == null) {
          print("No user");
          return const LoginScreen();
        } else {
          print("User: ${user.uid}");
          return HomeScreen();
        }
      },
      error: (error, stackTrace) => Text('Error: $error'),
      loading: () => CircularProgressIndicator(),
    );
  }
}
