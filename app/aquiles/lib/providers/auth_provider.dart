import 'package:aquiles/services/auth_services.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:hooks_riverpod/hooks_riverpod.dart';

final authServiceProvider = Provider<AuthServices>((ref) {
  return AuthServices(firebaseAuth: FirebaseAuth.instance);
});

final authStateProvider = StreamProvider<User?>(
  (ref) {
    final authService = ref.read(authServiceProvider);
    return authService.authStateChanges;
  },
);
