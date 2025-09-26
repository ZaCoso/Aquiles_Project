import 'package:cloud_firestore/cloud_firestore.dart';

class UserModel {
  final String userId;
  final String name;
  final String email;
  final String phoneNumber;
  final String age;
  final String surname;
  final String obraSocial;
  final String sexo;
  final String rol;
  final String estado;
  final String medico;
  final String ultimaConsulta;

  UserModel({
    required this.userId,
    required this.name,
    required this.surname,
    required this.email,
    required this.phoneNumber,
    required this.age,
    required this.obraSocial,
    required this.sexo,
    required this.rol,
    required this.estado,
    required this.medico,
    required this.ultimaConsulta,
  });

  factory UserModel.fromDocument(DocumentSnapshot doc) {
    final data = doc.data() as Map<String, dynamic>;
    return UserModel(
      userId: doc.id,
      name: data['name'] ?? '',
      surname: data['surname'] ?? '',
      email: data['email'] ?? '',
      phoneNumber: data['phoneNumber'] ?? '',
      age: data['age'] ?? '',
      obraSocial: data['obraSocial'] ?? '',
      sexo: data['sexo'] ?? '',
      rol: data['rol'] ?? '',
      estado: data['estado'] ?? '',
      medico: data['medico'] ?? '',
      ultimaConsulta: data['ultimaConsulta'] ?? '',
    );
  }

  factory UserModel.fromMap(Map<String, dynamic> data) {
    return UserModel(
      userId: data['userId'] ?? '',
      name: data['name'] ?? '',
      surname: data['surname'] ?? '',
      email: data['email'] ?? '',
      phoneNumber: data['phoneNumber'] ?? '',
      age: data['age'] ?? '',
      obraSocial: data['obraSocial'] ?? '',
      sexo: data['sexo'] ?? '',
      rol: data['rol'] ?? '',
      estado: data['estado'] ?? '',
      medico: data['medico'] ?? '',
      ultimaConsulta: data['ultimaConsulta'] ?? '',
    );
  }
}
