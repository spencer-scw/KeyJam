#include <Arduino.h>
#include <nlohmann/json.hpp>
#include <Keyboard.h>
#include <LittleFS.h>
#include <sstream>

//define pins here
#define DATA 28
#define LATCH 26
#define CLOCK 27

//define number of buttons
#define BTN_N 8

#define NEW_CONFIG false

#include "config.h"
#include "serialControl.h"
#include "shiftRegister.h"

using json = nlohmann::json;
using namespace nlohmann::literals;

json currentBaseConfig;
json currentProfileConfig;

std::vector<int> buttons( BTN_N );
std::vector<int> previousButtons( BTN_N );
SerialControl serialControl = SerialControl();
ShiftRegister buttonRegister = ShiftRegister( DATA, CLOCK, LATCH, BTN_N );

int getIndex( std::vector<std::string> v, const std::string &K ) {
    auto it = find( v.begin(), v.end(), K );
    if ( it != v.end()) {
        return it - v.begin();
    } else {
        return -1;
    }
}

void setup() {
    //If the config file does not exit, create it
    if ( LittleFS.begin() && (!LittleFS.exists( "/config.json" ) || NEW_CONFIG)) {
        initializeConfigFile();
    }

    //load in basic config
    File configFile = LittleFS.open( "/config.json", "r" );
    currentBaseConfig = json::parse( configFile.readString());
    configFile.close();

    //initialize the keyboard
    Keyboard.begin();

    //pinMode( 14, OUTPUT );
}

void loop() {
    for ( int i = 0; i < BTN_N; i++ ) {
        if ( buttons[i] == 1 && previousButtons[i] == 0 ) {
            previousButtons[i] = 1;
            if ( currentBaseConfig["buttons"][i].contains( "macro" ) && currentBaseConfig["buttons"][i]["macro"] ) {
                std::stringstream s;
                s << "press -k " << i;
                SerialControl::send( s.str());
            }
            //TODO press key
            //TODO trigger key combo
            //consumer keys
            if ( currentBaseConfig["buttons"][i].contains( "consumerKey" ) && currentBaseConfig["buttons"][i]["consumerKey"].is_number()) {
                Keyboard.consumerPress( currentBaseConfig["buttons"][i]["consumerKey"] );
            }
            //all other keys
            if ( currentBaseConfig["buttons"][i].contains( "keys" ) && !currentBaseConfig["buttons"][i]["keys"].empty()) {
                if ( currentBaseConfig["buttons"][i]["keys"].size() > 1 ||
                     (currentBaseConfig["buttons"][i].contains( "hold" ) && currentBaseConfig["buttons"][i]["hold"])) {
                    for ( const auto &j: currentBaseConfig["buttons"][i]["keys"] ) {
                        if ( j.is_number()) {
                            Keyboard.press( j );
                        } else {
                            Keyboard.press(((std::string) j).c_str()[0] );
                        }
                    }
                } else {
                    for ( const auto &j: currentBaseConfig["buttons"][i]["keys"] ) {
                        if ( j.is_number()) {
                            Keyboard.write( j );
                        } else {
                            Keyboard.write(((std::string) j).c_str()[0] );
                        }
                    }
                }
                if ( currentBaseConfig["buttons"][i]["keys"].size() > 1 &&
                     (!currentBaseConfig["buttons"][i].contains( "hold" ) || !currentBaseConfig["buttons"][i]["hold"])) {
                    for ( const auto &j: currentBaseConfig["buttons"][i]["keys"] ) {
                        if ( j.is_number()) {
                            Keyboard.release( j );
                        } else {
                            Keyboard.release(((std::string) j).c_str()[0] );
                        }
                    }
                }
            }
        } else if ( buttons[i] == 0 && previousButtons[i] == 1 ) {
            previousButtons[i] = 0;
            if ( currentBaseConfig["buttons"][i].contains( "hold" ) && currentBaseConfig["buttons"][i]["hold"] ) {
                for ( const auto &j: currentBaseConfig["buttons"][i]["keys"] ) {
                    if ( j.is_number()) {
                        Keyboard.release( j );
                    } else {
                        Keyboard.release(((std::string) j).c_str()[0] );
                    }
                }
            }
            if ( currentBaseConfig["buttons"][i].contains( "consumerKey" ) && currentBaseConfig["buttons"][i]["consumerKey"].is_number()) {
                Keyboard.consumerRelease();
            }
            //TODO release key (if needed)
        }

    }

    String msg = serialControl.check();
    if ( msg != "" ) {
        char cmd[32];
        char flag1[4];
        char option1[128];
        char flag2[4];
        char option2[128];
        char flag3[4];
        char option3[128];
        char flag4[4];
        char option4[128];
        int args = std::sscanf( msg.c_str(), "%s -%s %s -%s %s -%s %s -%s %s", cmd, flag1, option1, flag2, option2, flag3, option3, flag4, option4 );
        std::vector<std::string> params = {flag1, option1, flag2, option2, flag3, option3, flag4, option4};
        std::string command = cmd;
        if ( args > 0 && command == "update" ) {
            //TODO check for profiles
            if ( getIndex( params, "b" ) >= 0 ) {
                int inputIndex = 1 + getIndex( params, "b" );
                int valueIndex = 1 + getIndex( params, "j" );
                if ( inputIndex > 0 && valueIndex > 0 ) {
                    currentBaseConfig["buttons"][atoi( params[inputIndex].c_str())] = json::parse( params[valueIndex] );
                    saveConfigFile(currentBaseConfig);
                    SerialControl::send((String) "Updated Button" );
                }
            }
            //TODO Handle rotary encoders
        } else if ( command == "list" ) {
            if ( getIndex( params, "c" ) >= 0 && getIndex( params, "p" ) >= 0 && params[getIndex( params, "p" ) + 1] == "base" ) {
                SerialControl::send( currentBaseConfig.dump());
            } else if ( getIndex( params, "c" ) >= 0 && getIndex( params, "p" ) >= 0 && params[getIndex( params, "p" ) + 1] == "app" ) {
                SerialControl::send( currentProfileConfig.dump());
            }
        }
        //TODO do something with the command
    }

    delay( 10 );
}

void setup1() {
    //sets pin values
    buttonRegister.initialize();
}

void loop1() {
    buttonRegister.pollRegister( buttons );

    delay( 5 );
}
