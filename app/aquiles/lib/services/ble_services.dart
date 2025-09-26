import 'dart:async';
import 'dart:convert';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

class BleService {
  final FlutterReactiveBle _ble;
  final Uuid serviceUuid;
  final Uuid txUuid;
  final Uuid rxUuid;

  DiscoveredDevice? _device;
  QualifiedCharacteristic? _txChar;
  QualifiedCharacteristic? _rxChar;

  StreamSubscription<ConnectionStateUpdate>? _connectionSubscription;
  StreamSubscription<List<int>>? _notificationSubscription;
  final _notificationController = StreamController<String>.broadcast();
  bool _notificationsActive = false;

  BleService(this._ble, this.serviceUuid, this.txUuid, this.rxUuid);

  // Expose scan stream for UI to list available devices
  Stream<DiscoveredDevice> scan() {
    return _ble.scanForDevices(withServices: [serviceUuid]);
  }

  Future<bool> connectToDeviceId(String deviceId) async {
    final completer = Completer<bool>();
    _connectionSubscription = _ble.connectToDevice(
      id: deviceId,
      servicesWithCharacteristicsToDiscover: {serviceUuid: [txUuid, rxUuid]},
    ).listen((event) async {
      if (event.connectionState == DeviceConnectionState.connected) {
        _txChar = QualifiedCharacteristic(
          deviceId: deviceId,
          serviceId: serviceUuid,
          characteristicId: txUuid,
        );
        _rxChar = QualifiedCharacteristic(
          deviceId: deviceId,
          serviceId: serviceUuid,
          characteristicId: rxUuid,
        );
        _notificationSubscription = _ble
            .subscribeToCharacteristic(_rxChar!)
            .listen((data) {
          final message = utf8.decode(data);
          print('Mensaje recibido: $message');
          _notificationController.add(message);
        }, onError: (e, st) {
          // Ignore notification stream errors during teardown
          _notificationsActive = false;
        }, onDone: () {
          _notificationsActive = false;
        }, cancelOnError: true);
        _notificationsActive = true;
        final services = await _ble.discoverServices(deviceId);
        for (var service in services) {
          print('Service: ${service.serviceId}');
          for (var char in service.characteristics) {
            print('Characteristic: ${char.characteristicId}');
          }
        }
        completer.complete(true);
      } else if (event.connectionState == DeviceConnectionState.disconnected) {
        completer.complete(false);
      }
    });
    return completer.future;
  }

  Future<bool> connectToDevice(String nameToFind) async {
    final scanStream = _ble.scanForDevices(withServices: [serviceUuid]);

    await for (final device in scanStream) {
      if (device.name == nameToFind) {
        _device = device;

        final completer = Completer<bool>();

    _connectionSubscription = _ble.connectToDevice(
          id: device.id,
          servicesWithCharacteristicsToDiscover: {
            serviceUuid: [txUuid, rxUuid]
          },
        ).listen((event) async {
          if (event.connectionState == DeviceConnectionState.connected) {
            _txChar = QualifiedCharacteristic(
              deviceId: device.id,
              serviceId: serviceUuid,
              characteristicId: txUuid,
            );
            _rxChar = QualifiedCharacteristic(
              deviceId: device.id,
              serviceId: serviceUuid,
              characteristicId: rxUuid,
            );

            _notificationSubscription = _ble
                .subscribeToCharacteristic(_rxChar!)
                .listen((data) {
              final message = utf8.decode(data);
              print('Mensaje recibido: $message');
              _notificationController.add(message);
            }, onError: (e, st) {
              _notificationsActive = false;
            }, onDone: () {
              _notificationsActive = false;
            }, cancelOnError: true);
            _notificationsActive = true;
            final services = await _ble.discoverServices(device.id);
            for (var service in services) {
              print('Service: ${service.serviceId}');
              for (var char in service.characteristics) {
                print('Characteristic: ${char.characteristicId}');
              }
            }

            completer.complete(true);
          } else if (event.connectionState ==
              DeviceConnectionState.disconnected) {
            completer.complete(false);
          }
        });

        return completer.future;
      }
    }

    return false;
  }

  Future<void> send(String command) async {
    if (_txChar != null) {
      await _ble.writeCharacteristicWithResponse(
        _txChar!,
        value: utf8.encode(command),
      );
    }
  }

  Stream<String> getNotificationStream() {
    return _notificationController.stream;
  }

  Future<void> disconnect() async {
    // Cancel notifications subscription if still active.
    if (_notificationsActive) {
      try {
        await _notificationSubscription?.cancel();
      } catch (_) {
        // Ignore plugin errors on unsubscribe during disconnect
      }
    }
    _notificationSubscription = null;
    _notificationsActive = false;

    // Cancel connection subscription.
    try {
      await _connectionSubscription?.cancel();
    } catch (_) {
      // Ignore plugin errors on cancel
    }
    _connectionSubscription = null;

    _txChar = null;
    _rxChar = null;
    _device = null;
  }
}
