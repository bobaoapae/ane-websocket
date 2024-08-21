package br.com.redesurftank {
import air.net.WebSocket;

import flash.events.Event;
import flash.events.IOErrorEvent;
import flash.events.StatusEvent;
import flash.events.WebSocketEvent;
import flash.external.ExtensionContext;
import flash.net.Socket;
import flash.system.Capabilities;
import flash.utils.ByteArray;
import flash.utils.ByteArray;

public class AndroidWebSocket extends WebSocket {

    private var extContext:ExtensionContext = null;

    private var fallback:WebSocket = null;

    private var _closeReason:int = -1;

    private var _debugMode:Boolean;

    public function AndroidWebSocket() {
        super();
        initContext();
    }

    public static function get isSupported():Boolean {
        var plataform:String = Capabilities.version.substr(0, 3);
        switch (plataform) {
            case "AND":
                return true;
            case "WIN":
                return true;
            default:
                return false;
        }
    }

    override public function startServer(param1:Socket):void {
        throw new Error("AndroidWebSocket cannot take over an existing AS3 Socket object");
    }

    override public function get closeReason():int {
        if (fallback) {
            return fallback.closeReason;
        }
        return _closeReason;
    }

    override public function sendMessage(param1:uint, param2:*):void {
        if (fallback) {
            fallback.sendMessage(param1, param2);
            return;
        }
        if (extContext) {
            extContext.call("sendMessage", param1, param2);
        }
    }

    override public function close(param1:uint = 1000):void {
        if (fallback) {
            fallback.close(param1);
            return;
        }
        if (extContext) {
            _closeReason = param1;
            extContext.call("close", param1);
        }
    }

    override public function connect(param1:String, param2:Vector.<String> = null):void {
        if (fallback) {
            fallback.connect(param1, param2);
            return;
        }
        if (param2) {
            throw new Error("TODO: implement support for sending a protocol list");
        }
        if (extContext) {
            extContext.call("connect", param1, param2);
        }
    }

    override public function get protocol():String {
        if (fallback) {
            return fallback.protocol;
        }
        throw new Error("TODO: implement \'get protocol\'");
    }

    override public function set protocol(param1:String):void {
        if (fallback) {
            fallback.protocol = param1;
            return;
        }
        throw new Error("TODO: implement \'set protocol\'");
    }

    public function set debugMode(param1:Boolean):void {
        if (_debugMode != param1) {
            if (extContext) {
                _debugMode = extContext.call("SetDebugMode", param1) as Boolean;
            }
            if (param1) {
            }
        }
    }

    public function get debugMode():Boolean {
        return _debugMode;
    }

    private function Trace(param1:String):void {
        if (_debugMode) {
        }
    }

    private function initContext():void {
        if (isSupported) {
            extContext = ExtensionContext.createExtensionContext("br.com.redesurftank.androidwebsocket", "");
            if (extContext) {
                extContext.addEventListener("status", onStatusEvent);
            }
        }
        if (!extContext) {
            fallback = new WebSocket();
        }
    }

    private function onStatusEvent(param1:StatusEvent):void {
        var _loc2_:ByteArray = null;
        switch (param1.code) {
            case "connected":
                dispatchEvent(new Event("connect"));
                break;
            case "textMessage":
                _loc2_ = new ByteArray();
                _loc2_.writeUTFBytes(param1.level);
                dispatchEvent(new WebSocketEvent("websocketData", WebSocket.fmtTEXT, _loc2_));
                break;
            case "nextMessage":
                var bytes:ByteArray = extContext.call("getByteArrayMessage") as ByteArray;
                if (!bytes)
                    break;
                bytes.position = 0;
                dispatchEvent(new WebSocketEvent("websocketData", WebSocket.fmtBINARY, bytes));
                break;
            case "disconnected":
                var parameters:Array = param1.level.split(";");
                _closeReason = int(parameters[0]);
                dispatchEvent(new Event("close"));
                break;
            case "error":
                dispatchEvent(new IOErrorEvent("ioError", false, false, param1.level));
                break;
            default:
                throw new Error("TODO: handle StatusEvent code = [" + param1.code + "], level = [" + param1.level + "]");
        }
    }
}
}
