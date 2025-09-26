import 'package:aquiles/providers/auth_provider.dart';
import 'package:aquiles/providers/ble_connection_provider.dart';
import 'package:aquiles/screens/login_screen.dart';
import 'package:aquiles/widgets/performance_tab.dart';
import 'package:aquiles/widgets/settings_tab.dart';
import 'package:aquiles/widgets/status_tab.dart';
import 'package:aquiles/widgets/tutorials_tab.dart';
import 'package:aquiles/widgets/unconnected_status_tab.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

class HomeScreen extends ConsumerStatefulWidget {
  const HomeScreen({Key? key}) : super(key: key);

  @override
  ConsumerState<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends ConsumerState<HomeScreen>
    with SingleTickerProviderStateMixin {
  late TabController _tabController;
  bool _showSplash = true;

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 4, vsync: this);

    // Mostrar animación de splash por 2.5 segundos
    Future.delayed(const Duration(milliseconds: 2500), () {
      if (mounted) {
        setState(() {
          _showSplash = false;
        });
      }
    });
  }

  @override
  void dispose() {
    _tabController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final bleState = ref.watch(bleConnectionProvider);

    if (_showSplash) {
      return Scaffold(
        backgroundColor: const Color(0xFF0A1128),
        body: Center(
          child: AnimatedOpacity(
            opacity: 1.0,
            duration: const Duration(milliseconds: 500),
            child: Image.asset(
              'assets/images/logo.png',
              width: 150,
              height: 150,
            ),
          ),
        ),
      );
    }

    return Scaffold(
      backgroundColor: const Color(0xFF0A1128),
      appBar: AppBar(
        backgroundColor: const Color(0xFF0A1128).withOpacity(0.8),
        elevation: 0,
        toolbarHeight: 50, // Header más pequeño
        systemOverlayStyle: const SystemUiOverlayStyle(
          statusBarColor: Colors.transparent,
          statusBarIconBrightness: Brightness.light,
        ),
        title: Row(
          children: [
            Image.asset(
              'assets/images/logo.png',
              width: 70,
            ),
            const SizedBox(width: 8),
          ],
        ),
        actions: [
          IconButton(
            icon: const Icon(Icons.notifications_outlined,
                color: Color(0xFF8CCBD5)),
            onPressed: () {},
          ),
          IconButton(
            icon: const Icon(Icons.person_outline, color: Color(0xFF8CCBD5)),
            onPressed: () async {
              Navigator.pushNamed(context, "/profile");
            },
          ),
        ],
        bottom: TabBar(
          controller: _tabController,
          indicatorColor: const Color(0xFF3F797A),
          indicatorWeight: 3,
          labelColor: Colors.white,
          unselectedLabelColor: Colors.white.withOpacity(0.7),
          labelStyle:
              const TextStyle(fontSize: 12, fontWeight: FontWeight.bold),
          tabs: const [
            Tab(text: 'Estado'),
            Tab(text: 'Rendimiento'),
            Tab(text: 'Ajustes'),
            Tab(text: 'Tutoriales'),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabController,
        children: [
          switch (bleState) {
            BleConnectionState.connected => const StatusTab(),
            BleConnectionState.connecting =>
              const Center(child: CircularProgressIndicator()),
            BleConnectionState.error =>
              const UnconnectedStatusTab(), // Podés reemplazar por algo más informativo si querés
            _ => const UnconnectedStatusTab(),
          },
          PerformanceTab(),
          SettingsTab(),
          TutorialsTab()
        ],
      ),
    );
  }
}
