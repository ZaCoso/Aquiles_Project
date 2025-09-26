import 'package:aquiles/providers/auth_provider.dart';
import 'package:aquiles/providers/user_model_proivider.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

class ProfileScreen extends ConsumerWidget {
  const ProfileScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return Scaffold(
      appBar: AppBar(
        backgroundColor: Colors.transparent,
        shadowColor: Colors.transparent,
        surfaceTintColor: Colors.transparent,
        actions: [
          IconButton(
            icon: const Icon(Icons.logout, color: Color(0xFF8CCBD5)),
            tooltip: 'Cerrar sesión',
            onPressed: () async {
              try {
                await ref.read(authServiceProvider).signOut();
                // Vuelve a la ruta raíz (AuthChecker) para que muestre LoginScreen.
                if (Navigator.of(context).canPop()) {
                  Navigator.of(context)
                      .popUntil((route) => route.isFirst);
                }
              } catch (e) {
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(
                    content: Text('Error al cerrar sesión'),
                    backgroundColor: Colors.red,
                  ),
                );
              }
            },
          ),
        ],
      ),
      body: SingleChildScrollView(
        child: Padding(
          padding:
              const EdgeInsets.only(left: 20, right: 20, top: 20, bottom: 24),
          child: Container(
            width: double.infinity,
            decoration: BoxDecoration(
              gradient: LinearGradient(
                begin: Alignment.topLeft,
                end: Alignment.bottomRight,
                colors: [
                  const Color(0xFF0A1128),
                  const Color(0xFF295B62),
                ],
              ),
              borderRadius: BorderRadius.circular(16),
              border: Border.all(
                color: const Color(0xFF3F797A).withOpacity(0.5),
              ),
              boxShadow: [
                BoxShadow(
                  color: const Color(0xFF8CCBD5).withOpacity(0.15),
                  blurRadius: 15,
                  spreadRadius: 0,
                ),
              ],
            ),
            child: mainCard(buildInfo: _buildInfoRow),
          ),
        ),
      ),
    );
  }

  Widget _buildInfoRow(IconData icon, String text) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 8.0, horizontal: 20.0),
      child: Row(
        children: [
          Icon(icon, color: const Color(0xFF8CCBD5), size: 20),
          const SizedBox(width: 16),
          Expanded(
            child: Text(
              text,
              style: const TextStyle(color: Color(0xFFFFFFFA), fontSize: 16),
              overflow: TextOverflow.ellipsis,
            ),
          ),
        ],
      ),
    );
  }
}

class mainCard extends ConsumerWidget {
  final Function buildInfo;
  const mainCard({required this.buildInfo, super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return ref.watch(userProvider).when(
      data: (user) {
        return Column(
          children: [
            const SizedBox(height: 24),
            const CircleAvatar(
              radius: 50,
              child: Icon(
                Icons.person,
                size: 50,
                color: Color(0xFF3F797A),
              ),
            ),
            const SizedBox(height: 16),
            Text(
              "${user.name} ${user.surname} ",
              style: const TextStyle(
                color: Color(0xFFFFFFFA),
                fontSize: 24,
                fontWeight: FontWeight.bold,
              ),
            ),
            SizedBox(height: 24),
            buildInfo(Icons.email, user.email),
            buildInfo(Icons.phone, user.phoneNumber),
          ],
        );
      },
      error: (error, stackTrace) {
        print('Error fetching user data: $error');
        return Text("Error: ${error.toString()}");
      },
      loading: () {
        return const Center(child: CircularProgressIndicator());
      },
    );
  }
}
