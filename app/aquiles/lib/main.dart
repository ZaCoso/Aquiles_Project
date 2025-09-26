import 'package:aquiles/screens/auth_checker.dart';
import 'package:aquiles/screens/profile_screen.dart';
import 'package:aquiles/services/ble_services.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'screens/login_screen.dart';
import 'package:firebase_core/firebase_core.dart';
import 'firebase_options.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  SystemChrome.setPreferredOrientations([
    DeviceOrientation.portraitUp,
    DeviceOrientation.portraitDown,
  ]);

  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp(
    options: DefaultFirebaseOptions.currentPlatform,
  );

  runApp(
    ProviderScope(
      child: AquilesApp(),
    ),
  );
}

class AquilesApp extends StatelessWidget {
  AquilesApp({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Aquiles App',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        primaryColor: const Color(0xFF8CCBD5),
        scaffoldBackgroundColor: const Color(0xFF0A1128),
        fontFamily: 'Arial',
        textTheme: const TextTheme(
          bodyLarge: TextStyle(color: Color(0xFFFFFFFA)),
          bodyMedium: TextStyle(color: Color(0xFFFFFFFA)),
          titleLarge:
              TextStyle(color: Color(0xFF8CCBD5), fontWeight: FontWeight.bold),
          titleMedium:
              TextStyle(color: Color(0xFFFFFFFA), fontWeight: FontWeight.w500),
          labelMedium: TextStyle(color: Color(0xFFFFFFFA), fontSize: 14),
        ),
        colorScheme: ColorScheme.dark(
          primary: const Color(0xFF8CCBD5),
          secondary: const Color(0xFF3F797A),
          background: const Color(0xFF0A1128),
          surface: const Color(0xFF295B62).withOpacity(0.5),
          onPrimary: const Color(0xFFFFFFFA),
          onSecondary: const Color(0xFFFFFFFA),
          onBackground: const Color(0xFFFFFFFA),
          onSurface: const Color(0xFFFFFFFA),
        ),
      ),
      home: const AuthChecker(),
      routes: {
        '/login': (context) => const LoginScreen(),
        '/profile': (context) => const ProfileScreen(),
      },
    );
  }
}
