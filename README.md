# Relógio Zap

App Android para melhorar a integração do Huawei Watch Fit 4 com o celular, com foco em responder o WhatsApp por voz.

## Fase atual: teste do microfone do relógio

O Fit 4 não deixa apps de terceiros gravarem pelo microfone dele. Mas nas ligações ele funciona como fone Bluetooth (HFP).
Este APK testa se um app no celular consegue abrir esse canal (Bluetooth SCO) e gravar a voz pelo microfone do relógio.

O GitHub Actions compila o APK a cada push neste branch e o disponibiliza como artefato do workflow.
