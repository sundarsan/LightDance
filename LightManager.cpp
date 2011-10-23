#include "LightManager.h"

#include "LightController.h"
#include "LightInfo.h"
#include "LightProgram.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

  class LightManagerImpl : public LightManager {
    LightController *Controller;
    std::vector<LightInfo> LightSetup;

    std::vector<LightProgram *> AvailablePrograms;
    LightProgram *ActiveProgram;
    bool ChangeProgramRequested;

  protected:
    virtual LightController &GetController() const {
      return *Controller;
    }
    virtual const std::vector<LightInfo> &GetSetup() const {
      return LightSetup;
    }
    
  public:
    LightManagerImpl(LightController *Controller_,
                     std::vector<LightInfo> LightSetup_)
      : Controller(Controller_),
        LightSetup(LightSetup_),
        ActiveProgram(0),
        ChangeProgramRequested(false)
    {
      // Load the list of all programs.
      std::vector<LightProgram *> AllPrograms;
      LightProgram::LoadAllPrograms(AllPrograms);

      // Add the programs which are available given the current lighting setup.
      for (unsigned i = 0, e = AllPrograms.size(); i != e; ++i) {
        LightProgram *LP = AllPrograms[i];
        if (LP->WorksWithSetup(GetSetup())) {
          AvailablePrograms.push_back(LP);
        } else {
          fprintf(stderr, "ignoring light program: '%s' (not available)\n",
                  LP->GetName().c_str());
          delete LP;
        }
      }
        
      // FIXME: 
    }

    ~LightManagerImpl() {
      for (unsigned i = 0, e = AvailablePrograms.size(); i != e; ++i)
        delete AvailablePrograms[i];
      delete Controller;
    }

    virtual void ChangePrograms() {
      ChangeProgramRequested = true;
    }

    virtual void HandleBeat(MusicMonitorHandler::BeatKind Kind, double Time) {
      Controller->BeatNotification(Kind, Time);

      MaybeSwitchPrograms();

      ActiveProgram->HandleBeat(Kind, Time);
    }

    void MaybeSwitchPrograms() {
      // If a change was requested, stop the current program.
      if (ChangeProgramRequested && ActiveProgram) {
        ActiveProgram->Stop();
        ActiveProgram = 0;
      }

      // Otherwise, if there is no active program, select one.
      if (!ActiveProgram) {
        ChangeProgramRequested = false;

        ActiveProgram = AvailablePrograms[lrand48() % AvailablePrograms.size()];
        ActiveProgram->Start(*this);

        fprintf(stderr, "current program: '%s'\n",
                ActiveProgram->GetName().c_str());
      }
    }
  };

}

LightManager::LightManager() {}
LightManager::~LightManager() {}

LightManager *CreateLightManager(LightController *Controller,
                                 std::vector<LightInfo> LightSetup) {
  return new LightManagerImpl(Controller, LightSetup);
}
