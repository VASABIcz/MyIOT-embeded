#include <ESPmDNS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "ArduinoJson.h"
#include "Adafruit_SSD1306.h"

/*
 * instantiaite
 * provide device info
 * register service
 * register service
 * enable tcp
 * enable http
 * enable BLE
 * begin
 * user code
 * user can dynamicaly switch connections / services
 */

using namespace std;

auto ssid = "TP-LINK_41D52A";

WebServer server(80);
WiFiServer tcpServer(420);
// Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

enum BaseDataType {
    Float,
    String,
    Int,
    Bool,
    Invalid
};

struct BaseType {
    BaseDataType type;
    union {
        float f;
        const char* s;
        int i;
        bool b;
    };
};

enum EndpointType {
    Http,
    Tcp
};

struct EndpointInfo {
    string name;
    string route;
    string description;
    BaseDataType type;
    EndpointType endpointType;
    function<void(BaseType)> input;
    function<BaseType()> output;
};

static bool glow = false;
static int cislo = 0;
static string ajp = "UwU";
// FIXME this should be clearly HashMap
static vector<EndpointInfo> endpoints;
static string deviceName = "Moje Lampa";
static string identifier = "best-id-1234";
static string hostName = "uwu";
static vector<WiFiClient> tcpClients;

void filterConnections() {
    if (tcpClients.empty()) return;
    for (auto i = 0; i < tcpClients.size()-1; i++) {
        if (!tcpClients[i].connected()) {
            tcpClients.erase(tcpClients.begin()+i);
        }
    }
}

struct IOTManager {
public:
    static void BaseTypeToJson(BaseType b, string& str) {
        str.clear();
        string buf;

        switch (b.type) {
            case Bool:
                if (b.b)
                    str += R"({"value": true, "type": "bool"})";
                else
                    str += R"({"value": false, "type": "bool"})";
                break;
            case Int:
                str += "{\"value\": ";
                str += to_string(b.i);
                str += R"(, "type": "int"}))";
                break;
            case String:
                str += R"({"value": ")";
                buf = string(b.s);
                for (char i : buf) {
                    Serial.println(i);
                    if (i == '\n') {
                        str += "\\n";
                    }
                    else {
                        str += i;
                    }
                }
                str += R"(", "type": "string"}))";
                break;
            default:
                break;
        }
    }

    static BaseType jsonToBaseType(char* str) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, str);
        BaseType b{};

        string type = doc["type"];

        if (type == "bool") {
            bool value = doc["value"];
            b.type = Bool;
            b.b = value;
        }
        else if (type == "int") {
            int value = doc["value"];
            b.type = Int;
            b.i = value;
        }
        else if (type == "string") {
            string value = doc["value"];
            b.type = String;
            b.s = value.c_str();
        }
        else {
            b.type = Invalid;
        }
        return b;
    }

    static void endpointsToJson(string& res) {
        res.clear();
        res += "[";

        for (auto i = 0; i < endpoints.size(); i++) {
            auto e = endpoints[i];

            res += "{";

            res += R"("name": ")";
            res += e.name;
            res += "\",";

            res += R"("route": ")";
            res += e.route;
            res += "\",";

            res += R"("description": ")";
            res += e.description;
            res += "\",";

            res += R"("type": ")";
            switch (e.type) {
                case Bool:
                    res += "bool";
                    break;
                case Int:
                    res += "int";
                    break;
                case Float:
                    res += "float";
                    break;
                case String:
                    res += "string";
            }
            res += "\"";

            if (i == endpoints.size()-1) {
                res += "}";
            }
            else {
                res += "},";
            }
        }

        res += "]";
    }

    static void baseHttpRespond(BaseType x) {
        string parsed;

        BaseTypeToJson(x, parsed);
        if (parsed.length() == 0) {
            server.send(500, "we did fuckiee wakkiee 4.0");
            return;
        }

        server.send(200, "application/json", parsed.c_str());
    }


        static void begin() {
            MDNS.setInstanceName(deviceName.c_str());

            MDNS.addService("iotHttp", "tcp", 80);
            MDNS.addServiceTxt("iotHttp", "tcp", "identifier", identifier.c_str());

            MDNS.addService("iotTcp", "tcp", 420);
            MDNS.addServiceTxt("iotTcp", "tcp", "identifier", identifier.c_str());

            server.on("/", []() {
                server.send(200, "UwU", "UwU");
            });

            server.on("/api/capabilities", []() {
                string builder;

                endpointsToJson(builder);

                server.send(200, "application/json", builder.c_str());
            });
    }

    void registerIO(EndpointInfo info, function<void(BaseType)> input, function<BaseType()> output) {
        registerHttpIo(info, input, output);
        info.input = input;
        info.output = output;

        endpoints.push_back(info);
    }

    void registerHttpIo(const EndpointInfo& info, function<void(BaseType)> input, function<BaseType()> output) {
        server.on(Uri(info.route.c_str()), HTTPMethod::HTTP_GET, [output]() {
            Serial.println("=============");
            Serial.println("received GET");
            auto x = output();
            baseHttpRespond(x);
            Serial.println("finished GET");
        });

        server.on(Uri(info.route.c_str()), HTTPMethod::HTTP_POST, [input, output]() {
            // Serial.println("=============");
            // Serial.println("received POST");
            if (server.hasArg("plain") == false) {
                return;
            }
            auto body = server.arg("plain");


            auto data = jsonToBaseType(const_cast<char *>(body.c_str()));

            input(data);
            auto res = output();
            baseHttpRespond(res);
            // Serial.println("finished POST");
        });
    }

    void registerTcpIo(EndpointInfo info, function<void(BaseType)> input, function<BaseType()> output) {
        // TODO
    }
};

IOTManager manager;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
void sensorMock(void * parameter){
    while (true) {
        auto v = rand()/10000;
        BaseType data{
                Int,
                .i = v
        };

        string buf;
        IOTManager::BaseTypeToJson(data, buf);

        Serial.println("ahhh");
        for (auto &c: tcpClients) {
            Serial.println("before sending");
            if (!c.connected()) continue;

            Serial.println("sending");
            c.write("value /cisilko ");
            c.write(buf.c_str());
            c.println();
            // c.flush();
        }

        delay(300);
    }
}
#pragma clang diagnostic pop

void setup() {
    Serial.begin(115200);
    // Wire.begin();
    // display.begin();
    // display.setTextSize(1);
    // display.setTextColor(SSD1306_WHITE);
    // display.setCursor(0,0);

    // WiFi.begin(ssid, nullptr);
    WiFi.softAP("to jsem ja", nullptr);

    delay(1000);

    if(!MDNS.begin(hostName.c_str())) {
        Serial.println("Error starting mDNS");
        return;
    }

    //MDNS.addServiceTxt("iot", "tcp", "prop1", "test");
    // MDNS.addServiceTxt("iot", "tcp", "prop2", "test2");

    Serial.println(WiFi.localIP());

    manager.registerIO(EndpointInfo{"svetlo", "/switch", "switch the light", Bool}, [](BaseType t) {
        glow = t.b;

    }, []() -> BaseType {
        auto b = BaseType{};
        b.type = Bool;
        b.b = glow;
        return b;
    });

    manager.registerIO(EndpointInfo{"cisloTest", "/cisilko", "nastav cislo", Int}, [](BaseType t) {
        cislo = t.i;

    }, []() -> BaseType {
        auto b = BaseType{};
        b.type = Int;
        b.i = cislo;
        return b;
    });
    /*
    manager.registerIO(EndpointInfo{"svetlo2", "/switch", "switch the light", Bool}, [](BaseType t) {
        glow = t.b;

    }, []() -> BaseType {
        auto b = BaseType{};
        b.type = Bool;
        b.b = glow;
        return b;
    });

    manager.registerIO(EndpointInfo{"stringTest", "/text", "nastav ip", String}, [](BaseType t) {
        ajp = string(t.s);
    }, []() -> BaseType {
        auto b = BaseType{};
        b.type = String;
        b.s = ajp.c_str();
        return b;
    });
    */

    manager.begin();
    tcpServer.setNoDelay(true);
    server.begin();
    tcpServer.begin();
    tcpServer.setNoDelay(true);
    pinMode(26, OUTPUT);
    xTaskCreate(
            sensorMock,    // Function that should be called
            "sensor Mock",   // Name of the task (for debugging)
            10000,            // Stack size (bytes)
            nullptr,            // Parameter to pass
            1,               // Task priority
            nullptr             // Task handle
    );
}

int findFirst(int start, char c, class String& str) {
    for (int i = start; i < str.length(); ++i) {
        if (str[i] == c) return i;
    }
    return -1;
}

void handleTcp(WiFiClient& client) {
    if (!client.available()) return;
    Serial.println("handling client / waiting");

    // FIXME what if client doesn't send newline??
    auto msg = client.readStringUntil('\n');
    Serial.println("received message");
    Serial.printf("mess %s \n", msg.c_str());
    auto sep = findFirst(0, ' ', msg);
    if (sep == -1 && msg.startsWith("capabilities")) {
        client.write("capabilities ");

        string res;
        IOTManager::endpointsToJson(res);

        client.write(res.c_str());
        client.println();
    }
    if (sep == -1) {
        return;
    }

    if (msg.startsWith("get")) {
        Serial.println("received get");
        auto endpoint = find_if(endpoints.begin(), endpoints.end(), [&](EndpointInfo& it) {
            return msg.endsWith(it.route.c_str());
        });
        if (endpoint != end(endpoints)) {
            auto res = (*endpoint).output();
            string buf;
            IOTManager::BaseTypeToJson(res, buf);
            client.write("value ");
            client.write((*endpoint).route.c_str());
            client.write(" ");
            client.write(buf.c_str());
            client.println();
            // client.flush();

        }
    }
    else if (msg.startsWith("post")) {
        auto routeEndIndex = findFirst(5, ' ', msg);
        if (routeEndIndex == -1) return;
        auto route = msg.substring(5, routeEndIndex);
        auto data = msg.substring(routeEndIndex+1, msg.length());


        auto endpoint = find_if(endpoints.begin(), endpoints.end(), [&](EndpointInfo& it) {
            return strcmp(route.c_str(), it.route.c_str()) == 0;
        });
        if (endpoint != end(endpoints)) {
            Serial.println("e");
            Serial.println(route);
            Serial.println(data);
            (*endpoint).input(IOTManager::jsonToBaseType(const_cast<char *>(data.c_str())));
            auto res = (*endpoint).output();
            Serial.println("res is");
            printf("%d\n", res.type);
            string buf;
            IOTManager::BaseTypeToJson(res, buf);
            client.write("value ");
            client.write((*endpoint).route.c_str());
            client.write(" ");
            client.write(buf.c_str());
            client.println();
        }
        else {
            Serial.println("endpoint was null");
            Serial.println(route);
            Serial.println(data);
        }
    }
}

void serverHandleTcp() {
    auto client = tcpServer.available();

    if (client) {
        Serial.println("new client");
        tcpClients.push_back(client);
    }

    filterConnections();

    for (auto& c : tcpClients) {
        if (!c.connected()) continue;
        handleTcp(c);
    }
}

void loop() {
    /*
    display.setCursor(0,0);
    display.print(ajp.c_str());
    display.display();
    display.clearDisplay();
    */


    // Serial.println();
    serverHandleTcp();
    server.handleClient();
    // DoEvents(10);
    digitalWrite(26, glow);
    // Serial.println("update");
    delay(10);
}