import 'package:aquiles/models/user_model.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:cloud_firestore/cloud_firestore.dart';

class UserService {
  final FirebaseAuth _auth;
  final FirebaseFirestore _firestore;

  UserService({FirebaseAuth? auth, FirebaseFirestore? firestore})
      : _auth = auth ?? FirebaseAuth.instance,
        _firestore = firestore ?? FirebaseFirestore.instance;

  Future<void> createUser(String uid, Map<String, dynamic> userData) async {
    await _firestore.collection('users').doc(uid).set(userData);
  }

  Future<UserModel> getUser(String uid) async {
    final docRef = _firestore.collection('users').doc(uid);
    final doc = await docRef.get();

    if (!doc.exists) {
      final authUser = _auth.currentUser;
      final defaultData = <String, dynamic>{
        'name': (authUser?.displayName ?? '').split(' ').first,
        'surname': authUser?.displayName != null
            ? (authUser!.displayName!.split(' ').skip(1).join(' '))
            : '',
        'email': authUser?.email ?? '',
        'phoneNumber': authUser?.phoneNumber ?? '',
        'age': '',
        'obraSocial': '',
        'sexo': '',
        'rol': 'user',
        'estado': '',
        'medico': '',
        'ultimaConsulta': '',
      };

      await docRef.set(defaultData);

      final createdDoc = await docRef.get();
      return UserModel.fromDocument(createdDoc);
    }

    return UserModel.fromDocument(doc);
  }

  Future<void> updateUser(String uid, Map<String, dynamic> userData) async {
    await _firestore.collection('users').doc(uid).update(userData);
  }
}
