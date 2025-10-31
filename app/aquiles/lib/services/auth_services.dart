import 'package:firebase_auth/firebase_auth.dart';
import 'package:google_sign_in/google_sign_in.dart';

class AuthServices {
  FirebaseAuth firebaseAuth;

  AuthServices({
    required this.firebaseAuth,
  });

  Stream<User?> get authStateChanges => firebaseAuth.authStateChanges();
  Future<User?> signIn(String email, String password) async {
    try {
      final result = await firebaseAuth.signInWithEmailAndPassword(
        email: email,
        password: password,
      );
      print('User signed in: ${result.user?.email}');

      return result.user;
    } on FirebaseAuthException catch (e) {
      if (e.code == 'user-not-found') {
        print('No user found for that email.');
      } else if (e.code == 'wrong-password') {
        print('Wrong password provided for that user.');
      }
      rethrow;
    }
  }

  Future<User?> register(String email, String password) async {
    try {
      final result = await firebaseAuth.createUserWithEmailAndPassword(
        email: email,
        password: password,
      );
      print('User registered: ${result.user?.email}');
      return result.user;
    } on FirebaseAuthException catch (e) {
      switch (e.code) {
        case 'email-already-in-use':
          print('Email already in use.');
          break;
        case 'invalid-email':
          print('Invalid email.');
          break;
        case 'weak-password':
          print('Weak password provided.');
          break;
        default:
          print('Registration error: ${e.code}');
      }
      rethrow;
    }
  }

  Future<void> sendPasswordResetEmail(String email) async {
    try {
      await firebaseAuth.sendPasswordResetEmail(email: email);
      print('Password reset email sent to $email');
    } on FirebaseAuthException catch (e) {
      switch (e.code) {
        case 'user-not-found':
          print('No user found with that email.');
          break;
        case 'invalid-email':
          print('Invalid email format.');
          break;
        default:
          print('Error sending reset email: ${e.code}');
      }
      rethrow;
    }
  }

  Future<String?> getUserId() async {
    if (firebaseAuth.currentUser == null) {
      return null;
    }
    return firebaseAuth.currentUser?.uid;
  }

  Future<User?> signInWithGoogle() async {
    try {
      // Trigger the Google authentication flow
      final GoogleSignInAccount? googleUser = await GoogleSignIn().signIn();
      if (googleUser == null) return null; // user aborted

      final GoogleSignInAuthentication googleAuth =
          await googleUser.authentication;

      final credential = GoogleAuthProvider.credential(
        accessToken: googleAuth.accessToken,
        idToken: googleAuth.idToken,
      );

      final result = await firebaseAuth.signInWithCredential(credential);
      return result.user;
    } on FirebaseAuthException catch (e) {
      print('Google sign-in Firebase error: ${e.code}');
      rethrow;
    } catch (e) {
      print('Google sign-in error: $e');
      rethrow;
    }
  }

  Future<void> signOut() async {
    await firebaseAuth.signOut();
  }
}
