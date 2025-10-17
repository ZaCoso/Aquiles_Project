import 'package:aquiles/providers/ble_connection_provider.dart';
import 'package:aquiles/widgets/performance_tab.dart';
import 'package:aquiles/widgets/settings_tab.dart';
import 'package:aquiles/widgets/tutorials_tab.dart';
import 'package:aquiles/widgets/unconnected_status_tab.dart';
import 'package:flutter/material.dart';
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
  int _selectedTab = 0;

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 4, vsync: this);
    _tabController.addListener(() {
      if (!_tabController.indexIsChanging) {
        setState(() => _selectedTab = _tabController.index);
      }
    });

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
      backgroundColor: const Color(0xFF071120),
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            colors: [Color(0xFF071120), Color(0xFF10293B)],
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
          ),
        ),
        child: SafeArea(
          bottom: false,
          child: Column(
            children: [
              _HomeHeader(
                onProfileTap: () => Navigator.pushNamed(context, '/profile'),
              ),
              Expanded(
                child: TabBarView(
                  controller: _tabController,
                  physics: const NeverScrollableScrollPhysics(),
                  children: [
                    _StatusOverview(bleState: bleState),
                    PerformanceTab(),
                    SettingsTab(),
                    TutorialsTab(),
                  ],
                ),
              ),
              _HomeBottomNav(
                selectedIndex: _selectedTab,
                onItemSelected: (index) {
                  setState(() => _selectedTab = index);
                  _tabController.animateTo(index);
                },
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _HomeHeader extends StatelessWidget {
  const _HomeHeader({required this.onProfileTap});

  final VoidCallback onProfileTap;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.fromLTRB(24, 12, 24, 16),
      child: Row(
        children: [
          SizedBox(
            width: 50,
            height: 50,
            child: Image.asset('assets/images/logo.png'),
          ),
          const Spacer(),
          IconButton(
            icon: const Icon(Icons.notifications_outlined,
                color: Color(0xFF8CCBD5)),
            onPressed: () {},
          ),
          IconButton(
            icon: const Icon(Icons.person_outline, color: Color(0xFF8CCBD5)),
            onPressed: onProfileTap,
          ),
        ],
      ),
    );
  }
}

class _StatusOverview extends StatefulWidget {
  const _StatusOverview({required this.bleState});

  final BleConnectionState bleState;

  @override
  State<_StatusOverview> createState() => _StatusOverviewState();
}

class _StatusOverviewState extends State<_StatusOverview> {
  int _selectedMode = 0;

  static const _modes = ['Eco', 'Med', 'High', 'Power'];

  @override
  Widget build(BuildContext context) {
    if (widget.bleState == BleConnectionState.connecting) {
      return const Center(child: CircularProgressIndicator());
    }

    if (widget.bleState != BleConnectionState.connected) {
      return const UnconnectedStatusTab();
    }

    return SingleChildScrollView(
      padding: const EdgeInsets.fromLTRB(24, 0, 24, 32),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _HeroDevice(),
          const SizedBox(height: 32),
          _MetricsCard(),
          const SizedBox(height: 20),
          const _UpdateCard(),
          const SizedBox(height: 20),
          _ModeCard(
            selectedMode: _selectedMode,
            onModeSelected: (index) => setState(() => _selectedMode = index),
            modes: _modes,
          ),
        ],
      ),
    );
  }
}

class _HeroDevice extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(32),
        gradient: const RadialGradient(
          center: Alignment.topCenter,
          radius: 1.2,
          colors: [Color(0xFF143448), Color(0x00143448)],
        ),
      ),
      padding: const EdgeInsets.symmetric(vertical: 24, horizontal: 12),
      alignment: Alignment.center,
      child: Image.asset(
        'assets/images/exoesqueleto-1.png',
        height: 260,
        fit: BoxFit.contain,
      ),
    );
  }
}

class _MetricsCard extends StatelessWidget {
  const _MetricsCard();

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF103041).withOpacity(0.9),
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: const Color(0xFF4D7681).withOpacity(0.35)),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(0.35),
            offset: const Offset(0, 18),
            blurRadius: 32,
            spreadRadius: -18,
          ),
        ],
      ),
      padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 18),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: const [
          _MetricItem(
              icon: Icons.battery_full, label: '33%', color: Color(0xFF8CCBD5)),
          _MetricItem(
              icon: Icons.route, label: '33Km', color: Color(0xFF8CCBD5)),
          _MetricItem(
              icon: Icons.speed, label: '33Km/h', color: Color(0xFF8CCBD5)),
          _MetricItem(
              icon: Icons.accessibility_new,
              label: 'Eco Mode',
              color: Color(0xFF8CCBD5)),
        ],
      ),
    );
  }
}

class _MetricItem extends StatelessWidget {
  const _MetricItem({
    required this.icon,
    required this.label,
    required this.color,
  });

  final IconData icon;
  final String label;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(icon, color: color, size: 22),
        const SizedBox(height: 6),
        Text(
          label,
          style: const TextStyle(
            color: Color(0xFFE2EEF5),
            fontSize: 13,
            fontWeight: FontWeight.w600,
          ),
        ),
      ],
    );
  }
}

class _UpdateCard extends StatelessWidget {
  const _UpdateCard();

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF103041).withOpacity(0.9),
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: const Color(0xFF4D7681).withOpacity(0.35)),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(0.35),
            offset: const Offset(0, 18),
            blurRadius: 32,
            spreadRadius: -18,
          ),
        ],
      ),
      padding: const EdgeInsets.all(22),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            width: 42,
            height: 42,
            decoration: BoxDecoration(
              color: const Color(0xFF8CCBD5).withOpacity(0.18),
              borderRadius: BorderRadius.circular(14),
            ),
            child: const Icon(Icons.cloud_download_outlined,
                color: Color(0xFF8CCBD5)),
          ),
          const SizedBox(width: 18),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: const [
                Text(
                  'Actualización Disponible',
                  style: TextStyle(
                    color: Color(0xFFEAF5FF),
                    fontSize: 16,
                    fontWeight: FontWeight.w700,
                  ),
                ),
                SizedBox(height: 6),
                Text(
                  'Actualizá ahora para obtener un mejor rendimiento',
                  style: TextStyle(
                    color: Color(0xFFB2C8D6),
                    fontSize: 13,
                    height: 1.45,
                  ),
                ),
              ],
            ),
          ),
          IconButton(
            onPressed: () {},
            icon: const Icon(Icons.close, color: Color(0xFF8CCBD5), size: 20),
          ),
        ],
      ),
    );
  }
}

class _ModeCard extends StatelessWidget {
  const _ModeCard({
    required this.selectedMode,
    required this.onModeSelected,
    required this.modes,
  });

  final int selectedMode;
  final ValueChanged<int> onModeSelected;
  final List<String> modes;

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF103041).withOpacity(0.9),
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: const Color(0xFF4D7681).withOpacity(0.35)),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(0.35),
            offset: const Offset(0, 18),
            blurRadius: 32,
            spreadRadius: -18,
          ),
        ],
      ),
      padding: const EdgeInsets.fromLTRB(20, 20, 20, 18),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: List.generate(modes.length, (index) {
              final isSelected = selectedMode == index;
              return Expanded(
                child: GestureDetector(
                  onTap: () => onModeSelected(index),
                  child: AnimatedContainer(
                    duration: const Duration(milliseconds: 200),
                    margin: EdgeInsets.only(
                        right: index == modes.length - 1 ? 0 : 12),
                    padding: const EdgeInsets.symmetric(vertical: 12),
                    decoration: BoxDecoration(
                      color: isSelected
                          ? const Color(0xFF8CCBD5)
                          : const Color(0xFF143C4D),
                      borderRadius: BorderRadius.circular(14),
                    ),
                    alignment: Alignment.center,
                    child: Text(
                      modes[index],
                      style: TextStyle(
                        color: isSelected
                            ? const Color(0xFF0C1C2A)
                            : const Color(0xFFE0EDF6),
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ),
                ),
              );
            }),
          ),
          const SizedBox(height: 22),
          Text(
            modes[selectedMode],
            style: const TextStyle(
              color: Color(0xFFEAF5FF),
              fontSize: 18,
              fontWeight: FontWeight.w700,
            ),
          ),
          const SizedBox(height: 6),
          const Text(
            'Modo de bajo consumo',
            style: TextStyle(
              color: Color(0xFFB2C8D6),
              fontSize: 13,
              height: 1.4,
            ),
          ),
        ],
      ),
    );
  }
}

class _HomeBottomNav extends StatelessWidget {
  const _HomeBottomNav({
    required this.selectedIndex,
    required this.onItemSelected,
  });

  final int selectedIndex;
  final ValueChanged<int> onItemSelected;

  @override
  Widget build(BuildContext context) {
    const items = ['Estado', 'Rendimiento', 'Ajustes', 'Tutoriales'];
    final bottomInset = MediaQuery.of(context).padding.bottom;
    return Container(
      padding: EdgeInsets.fromLTRB(16, 10, 16, 20 + bottomInset),
      decoration: BoxDecoration(
        color: const Color(0xFF102030).withOpacity(0.85),
        borderRadius: const BorderRadius.vertical(top: Radius.circular(28)),
      ),
      child: Row(
        children: List.generate(items.length, (index) {
          final isSelected = selectedIndex == index;
          return Expanded(
            child: GestureDetector(
              behavior: HitTestBehavior.opaque,
              onTap: () => onItemSelected(index),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Text(
                    items[index],
                    style: TextStyle(
                      color: isSelected
                          ? const Color(0xFFEAF5FF)
                          : const Color(0xFF9CB6C6),
                      fontWeight:
                          isSelected ? FontWeight.w700 : FontWeight.w500,
                      fontSize: 13,
                    ),
                  ),
                  const SizedBox(height: 8),
                  AnimatedContainer(
                    duration: const Duration(milliseconds: 200),
                    height: 3,
                    width: isSelected ? 42 : 0,
                    decoration: BoxDecoration(
                      color: const Color(0xFF8CCBD5),
                      borderRadius: BorderRadius.circular(12),
                    ),
                  ),
                ],
              ),
            ),
          );
        }),
      ),
    );
  }
}
