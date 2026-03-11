#include "console.h"

Console::Console(): serial(Serial) {
}

Console::~Console()
{

}

String& Console::readCommand()
{
    static String command;
    command = "";
    while (true) {
        if (this->serial.available()) {
            char c = this->serial.read();
            if (c == '\n' || c == '\r') {
                break;
            } else if (c == '\b' || c == 127) {  // backspace or delete
                if (command.length() > 0) {
                    command.remove(command.length() - 1);
                }
            } else {
                command += c;
            }
        }
    }
    return command;
}
