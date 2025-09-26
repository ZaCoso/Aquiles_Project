import 'package:aquiles/models/user_model.dart';
import 'package:aquiles/providers/auth_provider.dart';
import 'package:aquiles/services/auth_services.dart';
import 'package:aquiles/services/user_service.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:hooks_riverpod/hooks_riverpod.dart';

final userServiceProvider = Provider((_) => UserService());

final userProvider = FutureProvider<UserModel>((ref) async {
  final auth = FirebaseAuth.instance; // o tu AuthService
  final uid = auth.currentUser!.uid;
  final service = ref.read(userServiceProvider);

  return await service.getUser(uid);
});
