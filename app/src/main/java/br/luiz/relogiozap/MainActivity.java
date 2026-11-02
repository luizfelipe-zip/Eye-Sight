package br.luiz.relogiozap;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.content.ComponentName;
import android.net.Uri;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.MediaRecorder;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * Teste: o relógio (pareado como fone Bluetooth/HFP, igual nas ligações)
 * consegue servir de microfone para um app no celular via Bluetooth SCO?
 */
public class MainActivity extends Activity {

    private AudioManager audio;
    private MediaRecorder recorder;
    private MediaPlayer player;
    private File lastFile;
    private TextView log;
    private ScrollView scroll;
    private Button btnWatch, btnPhone, btnStop, btnPlay, btnFake;
    private final Handler main = new Handler(Looper.getMainLooper());
    private boolean waitingSco = false;
    private TelecomManager telecom;
    private PhoneAccountHandle account;
    private BluetoothHeadset headset;
    private boolean fakeCallMode = false;

    private final BroadcastReceiver scoReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context c, Intent i) {
            int st = i.getIntExtra(AudioManager.EXTRA_SCO_AUDIO_STATE, -1);
            log("Estado SCO: " + scoName(st));
            if (st == AudioManager.SCO_AUDIO_STATE_CONNECTED && waitingSco) {
                waitingSco = false;
                startRecording(findInput(AudioDeviceInfo.TYPE_BLUETOOTH_SCO), "relogio");
            }
        }
    };

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        audio = (AudioManager) getSystemService(AUDIO_SERVICE);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int p = (int) (16 * getResources().getDisplayMetrics().density);
        root.setPadding(p, p, p, p);

        TextView title = new TextView(this);
        title.setText("Teste: gravar pelo microfone do relógio\n\n"
                + "1. Relógio conectado ao Poco (ligações pelo relógio ativadas).\n"
                + "2. Toque em \"Gravar pelo RELÓGIO\", fale perto do relógio com o celular longe, depois \"Parar\" e \"Tocar\".\n"
                + "3. Compare com \"Gravar pelo CELULAR\".\n"
                + "NOVO (v2): \"Chamada falsa + gravar pelo RELÓGIO\" simula uma chamada de internet para o relógio abrir o microfone.\n"
                + "Mande um print do log abaixo.");
        title.setTextSize(15);
        root.addView(title);

        btnWatch = button("Gravar pelo RELÓGIO", v -> recordFromWatch());
        btnFake = button("Chamada falsa + gravar pelo RELÓGIO", v -> fakeCallAndRecord());
        btnPhone = button("Gravar pelo CELULAR", v -> startRecording(findInput(AudioDeviceInfo.TYPE_BUILTIN_MIC), "celular"));
        btnStop = button("Parar", v -> stopRecording());
        btnPlay = button("Tocar última gravação", v -> play());
        root.addView(btnFake);
        root.addView(btnWatch);
        root.addView(btnPhone);
        root.addView(btnStop);
        root.addView(btnPlay);
        root.addView(button("Listar dispositivos de áudio", v -> listDevices()));

        log = new TextView(this);
        log.setTextIsSelectable(true);
        log.setTextSize(12);
        scroll = new ScrollView(this);
        scroll.addView(log);
        root.addView(scroll, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));
        setContentView(root);

        IntentFilter f = new IntentFilter(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED);
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(scoReceiver, f, Context.RECEIVER_EXPORTED);
        else registerReceiver(scoReceiver, f);

        log("Android " + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + "), "
                + Build.MANUFACTURER + " " + Build.MODEL);
        VoipConnectionService.logger = this::log;
        telecom = (TelecomManager) getSystemService(TELECOM_SERVICE);
        account = new PhoneAccountHandle(new ComponentName(this, VoipConnectionService.class), "relogiozap");
        try {
            telecom.registerPhoneAccount(PhoneAccount.builder(account, "Relógio Zap")
                    .setCapabilities(PhoneAccount.CAPABILITY_SELF_MANAGED)
                    .build());
            log("Conta de chamada (self-managed) registrada.");
        } catch (Exception e) {
            log("ERRO ao registrar conta de chamada: " + e);
        }
        askPermissions();
        updateButtons();
    }

    private Button button(String text, android.view.View.OnClickListener l) {
        Button bt = new Button(this);
        bt.setText(text);
        bt.setAllCaps(false);
        bt.setOnClickListener(l);
        return bt;
    }

    private void askPermissions() {
        List<String> need = new ArrayList<>();
        need.add(Manifest.permission.RECORD_AUDIO);
        if (Build.VERSION.SDK_INT >= 31) need.add(Manifest.permission.BLUETOOTH_CONNECT);
        need.add(Manifest.permission.MANAGE_OWN_CALLS);
        List<String> missing = new ArrayList<>();
        for (String s : need) if (checkSelfPermission(s) != PackageManager.PERMISSION_GRANTED) missing.add(s);
        if (!missing.isEmpty()) requestPermissions(missing.toArray(new String[0]), 1);
    }

    @Override
    public void onRequestPermissionsResult(int rc, String[] perms, int[] res) {
        for (int i = 0; i < perms.length; i++)
            log("Permissão " + perms[i].replace("android.permission.", "") + ": "
                    + (res[i] == PackageManager.PERMISSION_GRANTED ? "OK" : "NEGADA"));
        listDevices();
    }

    private void listDevices() {
        logHfp();
        log("--- Entradas de áudio ---");
        for (AudioDeviceInfo d : audio.getDevices(AudioManager.GET_DEVICES_INPUTS))
            log("  " + typeName(d.getType()) + " | " + d.getProductName() + " | id " + d.getId());
        if (Build.VERSION.SDK_INT >= 31) {
            log("--- Dispositivos de comunicação ---");
            for (AudioDeviceInfo d : audio.getAvailableCommunicationDevices())
                log("  " + typeName(d.getType()) + " | " + d.getProductName());
        }
    }

    private void logHfp() {
        try {
            if (headset == null) {
                BluetoothAdapter ad = ((BluetoothManager) getSystemService(BLUETOOTH_SERVICE)).getAdapter();
                if (ad == null) { log("Sem Bluetooth."); return; }
                ad.getProfileProxy(this, new BluetoothProfile.ServiceListener() {
                    @Override public void onServiceConnected(int p, BluetoothProfile prof) {
                        headset = (BluetoothHeadset) prof;
                        logHfp();
                    }
                    @Override public void onServiceDisconnected(int p) { headset = null; }
                }, BluetoothProfile.HEADSET);
                return;
            }
            List<BluetoothDevice> devs = headset.getConnectedDevices();
            log("--- Perfil de chamadas (HFP) conectado: " + devs.size() + " ---");
            for (BluetoothDevice d : devs)
                log("  " + d.getName() + " | áudio SCO " + (headset.isAudioConnected(d) ? "ABERTO" : "fechado"));
        } catch (SecurityException e) {
            log("Sem permissão de dispositivos próximos para ver o HFP.");
        }
    }

    private void fakeCallAndRecord() {
        if (recorder != null || VoipConnectionService.current != null) return;
        log("== Chamada falsa + gravar pelo relógio ==");
        logHfp();
        try {
            if (!telecom.isOutgoingCallPermitted(account)) log("Aviso: Telecom diz que não pode iniciar chamada agora.");
            Bundle extras = new Bundle();
            extras.putParcelable(TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE, account);
            telecom.placeCall(Uri.fromParts(PhoneAccount.SCHEME_SIP, "relogio", null), extras);
            log("placeCall enviado. Procurando o microfone do relógio por até 12 s...");
        } catch (SecurityException e) {
            log("ERRO de permissão ao iniciar chamada: " + e.getMessage());
            return;
        } catch (Exception e) {
            log("ERRO ao iniciar chamada: " + e);
            return;
        }
        fakeCallMode = true;
        pollSco(0);
    }

    private void pollSco(int tries) {
        main.postDelayed(() -> {
            if (!fakeCallMode || recorder != null) return;
            AudioDeviceInfo sco = findInput(AudioDeviceInfo.TYPE_BLUETOOTH_SCO);
            if (sco != null) {
                log("Microfone do relógio apareceu após ~" + (tries + 1) + " s: " + sco.getProductName());
                logHfp();
                startRecording(sco, "relogio");
            } else if (tries >= 11) {
                log("ERRO: o relógio não abriu o canal em 12 s. Encerrando a chamada falsa.");
                logHfp();
                listDevices();
                endFakeCall();
            } else {
                pollSco(tries + 1);
            }
        }, 1000);
    }

    private void endFakeCall() {
        fakeCallMode = false;
        VoipConnectionService.FakeCall c = VoipConnectionService.current;
        if (c != null) c.end();
    }

    private void recordFromWatch() {
        if (recorder != null) return;
        log("== Gravar pelo relógio ==");
        audio.setMode(AudioManager.MODE_IN_COMMUNICATION);
        if (Build.VERSION.SDK_INT >= 31) {
            AudioDeviceInfo sco = null;
            for (AudioDeviceInfo d : audio.getAvailableCommunicationDevices())
                if (d.getType() == AudioDeviceInfo.TYPE_BLUETOOTH_SCO) sco = d;
            if (sco == null) {
                log("ERRO: nenhum fone Bluetooth (SCO) disponível. O relógio está conectado e com ligações ativadas?");
                audio.setMode(AudioManager.MODE_NORMAL);
                return;
            }
            boolean ok = audio.setCommunicationDevice(sco);
            log("setCommunicationDevice(" + sco.getProductName() + ") = " + ok);
            // dá um tempo para o canal SCO abrir
            main.postDelayed(() -> {
                AudioDeviceInfo cur = audio.getCommunicationDevice();
                log("Dispositivo de comunicação atual: " + (cur == null ? "nenhum" : typeName(cur.getType()) + " " + cur.getProductName()));
                startRecording(findInput(AudioDeviceInfo.TYPE_BLUETOOTH_SCO), "relogio");
            }, 1500);
        } else {
            waitingSco = true;
            audio.startBluetoothSco();
            log("startBluetoothSco() chamado, aguardando conexão...");
            main.postDelayed(() -> {
                if (waitingSco) {
                    waitingSco = false;
                    log("ERRO: SCO não conectou em 5 s.");
                    releaseSco();
                }
            }, 5000);
        }
    }

    private AudioDeviceInfo findInput(int type) {
        for (AudioDeviceInfo d : audio.getDevices(AudioManager.GET_DEVICES_INPUTS))
            if (d.getType() == type) return d;
        return null;
    }

    private void startRecording(AudioDeviceInfo preferred, String tag) {
        if (recorder != null) return;
        try {
            String ts = new SimpleDateFormat("HHmmss", Locale.US).format(new Date());
            lastFile = new File(getExternalFilesDir(null), "teste_" + tag + "_" + ts + ".m4a");
            recorder = new MediaRecorder();
            recorder.setAudioSource("relogio".equals(tag)
                    ? MediaRecorder.AudioSource.VOICE_COMMUNICATION
                    : MediaRecorder.AudioSource.MIC);
            recorder.setOutputFormat(MediaRecorder.OutputFormat.MPEG_4);
            recorder.setAudioEncoder(MediaRecorder.AudioEncoder.AAC);
            recorder.setAudioSamplingRate(16000);
            recorder.setAudioEncodingBitRate(64000);
            recorder.setOutputFile(lastFile.getAbsolutePath());
            if (preferred != null) {
                boolean ok = recorder.setPreferredDevice(preferred);
                log("Preferir entrada " + typeName(preferred.getType()) + " (" + preferred.getProductName() + "): " + ok);
            } else {
                log("Aviso: entrada preferida não encontrada, usando padrão.");
            }
            recorder.prepare();
            recorder.start();
            main.postDelayed(() -> {
                if (recorder == null) return;
                AudioDeviceInfo r = recorder.getRoutedDevice();
                log("GRAVANDO. Entrada real em uso: " + (r == null ? "desconhecida" : typeName(r.getType()) + " (" + r.getProductName() + ")"));
            }, 700);
        } catch (Exception e) {
            log("ERRO ao gravar: " + e);
            if (recorder != null) { recorder.release(); recorder = null; }
            releaseSco();
        }
        updateButtons();
    }

    private void stopRecording() {
        if (recorder == null) return;
        try {
            recorder.stop();
            log("Gravação salva: " + lastFile.getName() + " (" + lastFile.length() / 1024 + " KB)");
        } catch (Exception e) {
            log("ERRO ao parar (gravação vazia?): " + e);
        }
        recorder.release();
        recorder = null;
        releaseSco();
        updateButtons();
    }

    private void releaseSco() {
        if (fakeCallMode) { endFakeCall(); return; }
        if (Build.VERSION.SDK_INT >= 31) audio.clearCommunicationDevice();
        else audio.stopBluetoothSco();
        audio.setMode(AudioManager.MODE_NORMAL);
    }

    private void play() {
        if (lastFile == null || !lastFile.exists()) { log("Nada gravado ainda."); return; }
        if (player != null) player.release();
        try {
            player = new MediaPlayer();
            player.setDataSource(lastFile.getAbsolutePath());
            player.prepare();
            player.start();
            log("Tocando " + lastFile.getName() + " (" + player.getDuration() / 1000 + " s) pelo alto-falante do celular");
        } catch (Exception e) {
            log("ERRO ao tocar: " + e);
        }
    }

    private void updateButtons() {
        boolean rec = recorder != null;
        btnWatch.setEnabled(!rec);
        btnFake.setEnabled(!rec);
        btnPhone.setEnabled(!rec);
        btnStop.setEnabled(rec);
        btnPlay.setEnabled(!rec);
    }

    private void log(String s) {
        main.post(() -> {
            String t = new SimpleDateFormat("HH:mm:ss", Locale.US).format(new Date());
            log.append(t + "  " + s + "\n");
            scroll.post(() -> scroll.fullScroll(ScrollView.FOCUS_DOWN));
        });
    }

    private static String scoName(int s) {
        switch (s) {
            case AudioManager.SCO_AUDIO_STATE_CONNECTED: return "CONECTADO";
            case AudioManager.SCO_AUDIO_STATE_CONNECTING: return "CONECTANDO";
            case AudioManager.SCO_AUDIO_STATE_DISCONNECTED: return "DESCONECTADO";
            case AudioManager.SCO_AUDIO_STATE_ERROR: return "ERRO";
            default: return String.valueOf(s);
        }
    }

    private static String typeName(int t) {
        switch (t) {
            case AudioDeviceInfo.TYPE_BUILTIN_MIC: return "MIC_CELULAR";
            case AudioDeviceInfo.TYPE_BLUETOOTH_SCO: return "BLUETOOTH_SCO";
            case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP: return "BLUETOOTH_A2DP";
            case AudioDeviceInfo.TYPE_BLE_HEADSET: return "BLE_HEADSET";
            case AudioDeviceInfo.TYPE_TELEPHONY: return "TELEFONIA";
            case AudioDeviceInfo.TYPE_WIRED_HEADSET: return "FONE_COM_FIO";
            case AudioDeviceInfo.TYPE_USB_DEVICE: return "USB";
            case AudioDeviceInfo.TYPE_REMOTE_SUBMIX: return "REMOTE_SUBMIX";
            case AudioDeviceInfo.TYPE_BUILTIN_EARPIECE: return "AURICULAR";
            case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER: return "ALTO_FALANTE";
            default: return "tipo_" + t;
        }
    }

    @Override
    protected void onDestroy() {
        stopRecording();
        if (player != null) player.release();
        unregisterReceiver(scoReceiver);
        endFakeCall();
        VoipConnectionService.logger = null;
        super.onDestroy();
    }
}
