void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
    if(type == WS_EVT_CONNECT){
        //client connected
        Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
        client->printf("Hello Client %u :)", client->id());
        client->ping();
    } else if(type == WS_EVT_DISCONNECT){
        //client disconnected
        Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id(), *((uint16_t*)arg));
    } else if(type == WS_EVT_ERROR){
        //error was received from the other end
        Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
    } else if(type == WS_EVT_PONG){
        //pong message was received (in response to a ping request maybe)
        Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
    } else if(type == WS_EVT_DATA){
        //data packet
        AwsFrameInfo * info = (AwsFrameInfo*)arg;
        if(info->final && info->index == 0 && info->len == len){
            //the whole message is in a single frame and we got all of it's data
            Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
            if(info->opcode == WS_TEXT){
                data[len] = 0;
                Serial.printf("%s\n", (char*)data);
            } else {
                for(size_t i=0; i < info->len; i++){
                    Serial.printf("%02x ", data[i]);
                }
                Serial.printf("\n");
            }
            if(info->opcode == WS_TEXT)
                client->text("I got your text message");
            else
                client->binary("I got your binary message");
        } else {
            //message is comprised of multiple frames or the frame is split into multiple packets
            if(info->index == 0){
                if(info->num == 0){
                    Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
                }
                Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
            }
            Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
            if(info->message_opcode == WS_TEXT){
                data[len] = 0;
                Serial.printf("%s\n", (char*)data);
            } else {
                for(size_t i=0; i < len; i++){
                    frame[i+info->index] = data[i];
                }
                Serial.printf("\n");
            }
            if((info->index + len) == info->len){
                Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
                if(info->final){
                    display.fillScreen(GxEPD_WHITE);
                    display.drawBitmap(frame, 0, 0, 200, 200, GxEPD_BLACK);
                    display.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
                    for (size_t i = 0; i < 5000; i++)
                    {
                        frame[i] = 0;
                    }
                    Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
                    if(info->message_opcode == WS_TEXT){
                        client->text("I got your text message");
                    } else {
                        client->binary("I got your binary message");
                    }
                }
            }
        }
    }
}