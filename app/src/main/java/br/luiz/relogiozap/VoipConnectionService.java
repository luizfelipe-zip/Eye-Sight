package br.luiz.relogiozap;

import android.os.Build;
import android.telecom.CallAudioState;
import android.telecom.CallEndpoint;
import android.telecom.Connection;
import android.telecom.ConnectionRequest;
import android.telecom.ConnectionService;
import android.telecom.DisconnectCause;
import android.telecom.PhoneAccountHandle;

import java.util.List;
import java.util.function.Consumer;

/**
 * "Chamada falsa" de internet (self-managed, como as chamadas do WhatsApp).
 * Serve só para fazer o Android/relógio abrirem o canal de áudio Bluetooth (HFP/SCO).
 */
public class VoipConnectionService extends ConnectionService {

    static volatile FakeCall current;
    static volatile Consumer<String> logger;

    static void log(String s) {
        Consumer<String> l = logger;
        if (l != null) l.accept(s);
    }

    @Override
    public Connection onCreateOutgoingConnection(PhoneAccountHandle acc, ConnectionRequest req) {
        log("Telecom criou a chamada falsa.");
        FakeCall c = new FakeCall();
        c.setAddress(req.getAddress(), android.telecom.TelecomManager.PRESENTATION_ALLOWED);
        c.setCallerDisplayName("Relógio Zap", android.telecom.TelecomManager.PRESENTATION_ALLOWED);
        c.setActive();
        current = c;
        return c;
    }

    @Override
    public void onCreateOutgoingConnectionFailed(PhoneAccountHandle acc, ConnectionRequest req) {
        log("ERRO: Telecom recusou a chamada falsa.");
    }

    static class FakeCall extends Connection {
        FakeCall() {
            setConnectionProperties(PROPERTY_SELF_MANAGED);
            setAudioModeIsVoip(true);
            setConnectionCapabilities(CAPABILITY_HOLD | CAPABILITY_SUPPORT_HOLD);
        }

        @Override
        public void onCallAudioStateChanged(CallAudioState st) {
            log("Rota de áudio da chamada: " + CallAudioState.audioRouteToString(st.getRoute())
                    + " (suportadas: " + st.getSupportedRouteMask() + ")");
            if (Build.VERSION.SDK_INT < 34
                    && (st.getSupportedRouteMask() & CallAudioState.ROUTE_BLUETOOTH) != 0
                    && st.getRoute() != CallAudioState.ROUTE_BLUETOOTH) {
                log("Pedindo rota BLUETOOTH...");
                setAudioRoute(CallAudioState.ROUTE_BLUETOOTH);
            }
        }

        @Override
        public void onAvailableCallEndpointsChanged(List<CallEndpoint> eps) {
            StringBuilder sb = new StringBuilder("Saídas disponíveis na chamada:");
            CallEndpoint bt = null;
            for (CallEndpoint e : eps) {
                sb.append(" [").append(e.getEndpointName()).append(" tipo ").append(e.getEndpointType()).append("]");
                if (e.getEndpointType() == CallEndpoint.TYPE_BLUETOOTH) bt = e;
            }
            log(sb.toString());
            if (bt != null) {
                log("Pedindo saída Bluetooth: " + bt.getEndpointName());
                requestCallEndpointChange(bt, Runnable::run, new android.os.OutcomeReceiver<Void, android.telecom.CallEndpointException>() {
                    @Override public void onResult(Void r) { log("Saída Bluetooth ativada."); }
                    @Override public void onError(android.telecom.CallEndpointException e) { log("ERRO ao trocar saída: " + e.getCode()); }
                });
            }
        }

        @Override
        public void onCallEndpointChanged(CallEndpoint ep) {
            log("Saída atual da chamada: " + ep.getEndpointName() + " (tipo " + ep.getEndpointType() + ")");
        }

        @Override
        public void onDisconnect() { end(); }

        @Override
        public void onAbort() { end(); }

        void end() {
            setDisconnected(new DisconnectCause(DisconnectCause.LOCAL));
            destroy();
            if (current == this) current = null;
            log("Chamada falsa encerrada.");
        }
    }
}
