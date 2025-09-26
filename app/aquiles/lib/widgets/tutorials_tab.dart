import 'package:flutter/material.dart';

class TutorialsTab extends StatelessWidget {
  const TutorialsTab({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            'Tutoriales',
            style: Theme.of(context).textTheme.titleLarge?.copyWith(
                  fontSize: 20,
                  fontWeight: FontWeight.bold,
                ),
          ),
          const SizedBox(height: 16),
          Text(
            'Aprende a usar tu exoesqueleto con nuestros videos tutoriales',
            style: TextStyle(
              color: Colors.white.withOpacity(0.7),
              fontSize: 14,
            ),
          ),
          const SizedBox(height: 24),
          _buildTutorialCard(
            context,
            'Primeros pasos',
            'Aprende lo básico para comenzar a usar tu exoesqueleto',
            '5:32',
            Icons.play_circle_fill,
          ),
          const SizedBox(height: 16),
          _buildTutorialCard(
            context,
            'Configuración avanzada',
            'Personaliza tu exoesqueleto para un mejor rendimiento',
            '8:45',
            Icons.settings,
          ),
          const SizedBox(height: 16),
          _buildTutorialCard(
            context,
            'Mantenimiento',
            'Mantén tu exoesqueleto en óptimas condiciones',
            '7:18',
            Icons.build,
          ),
          const SizedBox(height: 16),
          _buildTutorialCard(
            context,
            'Solución de problemas',
            'Resuelve los problemas más comunes',
            '10:23',
            Icons.help_outline,
          ),
          const SizedBox(height: 16),
          _buildTutorialCard(
            context,
            'Ejercicios recomendados',
            'Rutinas de ejercicios para mejorar tu movilidad',
            '12:05',
            Icons.fitness_center,
          ),
        ],
      ),
    );
  }

  Widget _buildTutorialCard(
    BuildContext context,
    String title,
    String description,
    String duration,
    IconData icon,
  ) {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF0A1128).withOpacity(0.8),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: const Color(0xFF3F797A).withOpacity(0.3),
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            height: 160,
            decoration: BoxDecoration(
              borderRadius: const BorderRadius.vertical(
                top: Radius.circular(12),
              ),
              gradient: LinearGradient(
                begin: Alignment.topCenter,
                end: Alignment.bottomCenter,
                colors: [
                  const Color(0xFF295B62),
                  const Color(0xFF0A1128),
                ],
              ),
            ),
            child: Stack(
              alignment: Alignment.center,
              children: [
                Icon(
                  icon,
                  size: 48,
                  color: const Color(0xFF8CCBD5).withOpacity(0.3),
                ),
                Center(
                  child: Container(
                    width: 60,
                    height: 60,
                    decoration: BoxDecoration(
                      color: const Color(0xFF8CCBD5).withOpacity(0.2),
                      shape: BoxShape.circle,
                    ),
                    child: const Center(
                      child: Icon(
                        Icons.play_arrow,
                        color: Colors.white,
                        size: 36,
                      ),
                    ),
                  ),
                ),
                Positioned(
                  bottom: 8,
                  right: 8,
                  child: Container(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 8,
                      vertical: 4,
                    ),
                    decoration: BoxDecoration(
                      color: Colors.black.withOpacity(0.6),
                      borderRadius: BorderRadius.circular(4),
                    ),
                    child: Text(
                      duration,
                      style: const TextStyle(
                        color: Colors.white,
                        fontSize: 12,
                        fontWeight: FontWeight.w500,
                      ),
                    ),
                  ),
                ),
              ],
            ),
          ),
          Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: const TextStyle(
                    color: Color(0xFF8CCBD5),
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 8),
                Text(
                  description,
                  style: TextStyle(
                    color: Colors.white.withOpacity(0.7),
                    fontSize: 14,
                  ),
                ),
                const SizedBox(height: 16),
                Row(
                  children: [
                    ElevatedButton.icon(
                      onPressed: () {},
                      icon: const Icon(Icons.play_arrow),
                      label: const Text('Ver tutorial'),
                      style: ElevatedButton.styleFrom(
                        backgroundColor: const Color(0xFF3F797A),
                        foregroundColor: Colors.white,
                        elevation: 0,
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(8),
                        ),
                      ),
                    ),
                    const Spacer(),
                    IconButton(
                      onPressed: () {},
                      icon: const Icon(
                        Icons.bookmark_border,
                        color: Color(0xFF8CCBD5),
                      ),
                    ),
                    IconButton(
                      onPressed: () {},
                      icon: const Icon(
                        Icons.share,
                        color: Color(0xFF8CCBD5),
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
