#include "LightProgram.h"

#include "LightController.h"
#include "LightManager.h"

#include <cassert>
#include <vector>

namespace {

  class LightProgramImpl : public LightProgram {
    LightManager *ActiveManager;
    std::vector<int> ActiveAssignments;

    int which_light;
    double last_change_time;

  public:
    LightProgramImpl() : ActiveManager(0) {
      which_light = 0;
      last_change_time = -1;
    }

    virtual bool WorksWithSetup(const std::vector<LightInfo> &Lights) const {
      // FIXME: Implement.
      return true;
    }

    virtual std::string GetName() const {
      return "default program";
    }

    virtual void Start(LightManager &Manager) {
      assert(ActiveManager == 0 && ActiveAssignments.empty() &&
             "LightProgram is already active!");

      ActiveManager = &Manager;

      // FIXME: Create a random light assignment.
      ActiveAssignments.push_back(0);
      ActiveAssignments.push_back(2);
    }
    virtual void Stop() {
      ActiveManager = 0;
      ActiveAssignments.clear();
    }

    LightManager &GetManager() const {
      assert(ActiveManager && "LightProgram is not active!");
      return *ActiveManager;
    }

    LightController &GetController() const {
      return GetManager().GetController();
    }

    virtual void HandleBeat(MusicMonitorHandler::BeatKind Kind, double time) {
      if (time - last_change_time > 0.01) {
        last_change_time = time;
        which_light = !which_light;

        GetController().SetLight(0, which_light == 0);
        GetController().SetLight(2, which_light == 1);
      }
    }
  };
}

LightProgram::LightProgram() {}
LightProgram::~LightProgram() {}

void LightProgram::LoadAllPrograms(std::vector<LightProgram *> &Result) {
  Result.push_back(new LightProgramImpl());
}


