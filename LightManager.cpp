#include "LightManager.h"

#include "LightController.h"
#include "LightInfo.h"
#include "LightProgram.h"
#include "Util.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

  class LightManagerImpl : public LightManager {
    LightController *Controller;
    std::vector<LightInfo> LightSetup;

    std::vector<LightProgram *> AvailablePrograms;
    std::vector<LightState> LightStates;
    LightProgram *ActiveProgram;
    bool ChangeProgramRequested;

    double RecentBeatTimes[64];
    unsigned RecentBeatPosition, NumRecentBeatTimes;

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
        ChangeProgramRequested(false),
        RecentBeatTimes(),
        RecentBeatPosition(0),
        NumRecentBeatTimes(sizeof(RecentBeatTimes) / sizeof(RecentBeatTimes[0]))
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

      for (unsigned i = 0, e = LightSetup.size(); i != e; ++i)
        LightStates.push_back(LightState());
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
      RecentBeatTimes[RecentBeatPosition % NumRecentBeatTimes] =
        get_elapsed_time_in_seconds();
      ++RecentBeatPosition;

      Controller->BeatNotification(Kind, Time);

      MaybeSwitchPrograms();

      ActiveProgram->HandleBeat(Kind, Time);
    }

    virtual void SetLight(unsigned Index, bool Enable) {
      assert(Index < LightStates.size() && "Invalid index");
      Controller->SetLight(LightSetup[Index].Index, Enable);

      LightState &State = LightStates[Index];
      if (Enable != State.Enabled) {
        double Elapsed = get_elapsed_time_in_seconds();

        State.Enabled = Enable;
        if (Enable) {
          State.LastEnableTime = Elapsed;
        } else {
          State.TotalEnabledTime += Elapsed - State.LastEnableTime;
        }
      }
    }

    virtual const LightState &GetLightState(unsigned Index) const {
      assert(Index < LightStates.size() && "Invalid index");
      return LightStates[Index];
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

        // Compute the current selection rating for each program.
        std::vector<double> Ratings;
        double SumRatings = 0.0;
        for (unsigned i = 0, e = AvailablePrograms.size(); i != e; ++i) {
          double Rating = AvailablePrograms[i]->GetRating(*this);
          Ratings.push_back(Rating);
          SumRatings += Rating;
        }

        // Select a program based on the weighted probability.
        double PickValue = drand48() * SumRatings;
        for (unsigned i = 0, e = AvailablePrograms.size(); i != e; ++i) {
          PickValue -= Ratings[i];

          if (PickValue <= 0.0 || i == e - 1) {
            ActiveProgram = AvailablePrograms[i];
            break;
          }
        }
        assert(ActiveProgram && "random selection failed!");

        // Start the program.
        ActiveProgram->Start(*this);
        fprintf(stderr, "current program: '%s'\n",
                ActiveProgram->GetName().c_str());
      }
    }

    virtual double GetRecentBPM() const {
      unsigned NumBeats = std::min(RecentBeatPosition, NumRecentBeatTimes);
      double OldestTime = RecentBeatTimes[
        (RecentBeatPosition + 1) % NumRecentBeatTimes];

      return 60 * NumBeats / (get_elapsed_time_in_seconds() - OldestTime);
    }

    virtual std::string GetProgramName() const {
      if (ActiveProgram)
        return ActiveProgram->GetName();
      return "(no active program)";
    }
  };

}

LightManager::LightManager() {}
LightManager::~LightManager() {}

LightManager *CreateLightManager(LightController *Controller,
                                 std::vector<LightInfo> LightSetup) {
  return new LightManagerImpl(Controller, LightSetup);
}
