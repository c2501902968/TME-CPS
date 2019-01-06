#ifndef _MINION_H_
#define _MINION_H_

#include <iostream>
#include <thread>
#include <map>
#include "../Application/Application.h"

using namespace std;

class Minion
{
  private:
    string ipAddress;
    map<string, Application*> minionApplications;

    public:
      Minion();
      Minion(std::string ipAddress);
      Minion(const Minion&);

      string getIpAddress();
      map<string, Application*> getApplications();

      void addApp(Application* app);
      void removeApp(string appID);
};

#endif