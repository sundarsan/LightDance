#include "LightProgram.h"

#include "LightInfo.h"
#include "LightManager.h"
#include "Util.h"

#include <cassert>
#include <cmath>
#include <vector>

namespace {
  class ChannelProgram;
  class LightProgramImpl;

  class ChannelAction {
  public:
    struct ActionResult {
    public:
      enum ResultKind {
        Retry,
        Advance,
        Next,
        Goto,
        SwitchPrograms
      } Kind;
      unsigned Value;

    private:
      static ActionResult Make(ResultKind Kind, unsigned Value = 0) {
        ActionResult Result = { Kind, Value };
        return Result;
      }

    public:
      static ActionResult MakeRetry() {
        return Make(Retry);
      }
      static ActionResult MakeAdvance() {
        return Make(Advance);
      }
      static ActionResult MakeNext() {
        return Make(Next);
      }
      static ActionResult MakeGoto(unsigned Index) {
        return Make(Goto, Index);
      }
      static ActionResult MakeSwitchPrograms() {
        return Make(SwitchPrograms);
      }
    };

  public:
    virtual ~ChannelAction() {}

    virtual void Start() {};

    virtual void Initialize(ChannelProgram &Program) {};

    virtual ActionResult Step(MusicMonitorHandler::BeatKind Kind,
                              ChannelProgram &Program) = 0;
  };

  class ChannelProgram {
    LightProgramImpl *ActiveProgram;
    unsigned ActiveLightIndex;

    std::vector<ChannelAction *> Actions;
    unsigned Position;
    bool NeedsStrobe;

  public:
    ChannelProgram(bool NeedsStrobe_ = false)
      : ActiveProgram(0), ActiveLightIndex(0), Position(0),
        NeedsStrobe(NeedsStrobe_) {}

    ~ChannelProgram() {
      while (!Actions.empty()) {
        delete Actions.back();
        Actions.pop_back();
      }
    }

    LightProgramImpl &GetProgram() {
      assert(ActiveProgram && "Channel program is not active!");
      return *ActiveProgram;
    }

    LightManager &GetManager();

    std::vector<ChannelAction*> &GetActions() { return Actions; }

    void Start(unsigned LightIndex, LightProgramImpl &Program) {
      assert(!Actions.empty() && "Channel program has no actions!");

      ActiveProgram = &Program;
      ActiveLightIndex = LightIndex;
      
      Position = 0;
      for (unsigned i = 0, e = Actions.size(); i != e; ++i)
        Actions[i]->Initialize(*this);
      Actions[0]->Start();
    }
    void Stop() {
      ActiveProgram = 0;
    }

    unsigned GetPosition() {
      return Position;
    }

    void Step(MusicMonitorHandler::BeatKind Kind);

    void GotoPosition(unsigned NewPosition) {
      assert(NewPosition < Actions.size() && "Invalid position!");

      // If we are jumping backwards, reinitialize all the actions in between.
      for (unsigned i = NewPosition + 1, e = Position; i < e; ++i) {
        Actions[i]->Initialize(*this);
      }

      // Update the position.
      Position = NewPosition;
      Actions[Position]->Start();
    }

    const LightState &GetLightState() {
      return GetManager().GetLightState(ActiveLightIndex);
    }

    void SetLight(bool Enable) {
      return GetManager().SetLight(ActiveLightIndex, Enable);
    }

    const LightState &GetLightState(bool Enable) {
      return GetManager().GetLightState(ActiveLightIndex);
    }

    bool WorksWithLight(const LightInfo &Info) const {
      if (NeedsStrobe)
        return Info.isStrobe();

      return !Info.isStrobe();
    }
  };

  class LightProgramImpl : public LightProgram {
    LightManager *ActiveManager;
    std::vector<int> ActiveAssignments;
    double ActiveStartTime;
    double ActiveBeatElapsed;

    /// The program name.
    std::string Name;

    /// The maximum amount of time to run this program before requesting the
    /// manager switch programs.
    double MaxProgramTime;

    /// The program to run for each light channel.
    std::vector<ChannelProgram *> ChannelPrograms;

    double ShortestBeatInterval;

  public:
    LightProgramImpl(std::string Name_, double MaxProgramTime_,
                     std::vector<ChannelProgram *> ChannelPrograms_,
                     double ShortestBeatInterval_ = 0.03)
      : ActiveManager(0),
        ActiveStartTime(-1),
        ActiveBeatElapsed(-1),
        Name(Name_),
        MaxProgramTime(MaxProgramTime_),
        ChannelPrograms(ChannelPrograms_),
        ShortestBeatInterval(ShortestBeatInterval_)
    {
    }

    virtual bool WorksWithSetup(const std::vector<LightInfo> &Lights) const {
      // FIXME: Implement.
      return true;
    }

    virtual std::string GetName() const {
      return Name;
    }

    virtual void Start(LightManager &Manager) {
      assert(ActiveManager == 0 && ActiveAssignments.empty() &&
             "LightProgram is already active!");

      ActiveManager = &Manager;
      ActiveStartTime = get_elapsed_time_in_seconds();
      ActiveBeatElapsed = -1;

      // Create the light assignment. This isn't really good enough, as it could
      // fail to find assignments if programs got more complicated (could assign
      // to multiple light types). Good enough for now though.
      std::vector<LightInfo> AvailableLights = Manager.GetSetup();
      for (unsigned i = 0, e = ChannelPrograms.size(); i != e; ++i) {
        std::vector<unsigned> UsableLights;
        ChannelProgram *Program = ChannelPrograms[i];

        // Determine the lights that this program can use.
        for (unsigned j = 0; j != AvailableLights.size(); ++j) {
          if (Program->WorksWithLight(AvailableLights[j]))
            UsableLights.push_back(j);
        }

        assert(!UsableLights.empty() &&
               "unable to compute light assignment!");
        unsigned Index = int(floorf(drand48() * UsableLights.size()));

        ActiveAssignments.push_back(AvailableLights[UsableLights[Index]].Index);
        AvailableLights.erase(AvailableLights.begin() + UsableLights[Index]);
      }

      for (unsigned i = 0, e = ChannelPrograms.size(); i != e; ++i) {
        ChannelPrograms[i]->Start(ActiveAssignments[i], *this);
      }

      // Turn off any lights which aren't assigned.
      for (unsigned i = 0, e = AvailableLights.size(); i != e; ++i) {
        GetManager().SetLight(AvailableLights[i].Index, false);
      }
    }
    virtual void Stop() {
      for (unsigned i = 0, e = ChannelPrograms.size(); i != e; ++i) {
        ChannelPrograms[i]->Stop();
      }

      ActiveManager = 0;
      ActiveAssignments.clear();
    }

    double GetActiveBeatElapsed() const {
      return ActiveBeatElapsed;
    }

    LightManager &GetManager() const {
      assert(ActiveManager && "LightProgram is not active!");
      return *ActiveManager;
    }

    void SetChannel(unsigned ChannelIndex, bool Enable) {
      assert(ChannelIndex < ActiveAssignments.size() &&
             "Invalid channel index");
      int LightIndex = ActiveAssignments[ChannelIndex];
      if (LightIndex == -1)
        return;

      GetManager().SetLight(LightIndex, Enable);
    }

    virtual void HandleBeat(MusicMonitorHandler::BeatKind Kind, double time) {
      double Elapsed = get_elapsed_time_in_seconds() - ActiveStartTime;

      // For now, we globally filter out beats that are too fast. I haven't
      // figured out a better place to incorporate this yet.
      if (Elapsed - ActiveBeatElapsed < ShortestBeatInterval)
        return;

      ActiveBeatElapsed = Elapsed;

      for (unsigned i = 0, e = ChannelPrograms.size(); i != e; ++i)
        ChannelPrograms[i]->Step(Kind);

      if (ActiveBeatElapsed > MaxProgramTime)
        GetManager().ChangePrograms();
    }
  };
}

LightProgram::LightProgram() {}
LightProgram::~LightProgram() {}

LightManager &ChannelProgram::GetManager() {
  return GetProgram().GetManager();
}

void ChannelProgram::Step(MusicMonitorHandler::BeatKind Kind) {
  // Execute program actions in a loop.
  for (;;) {
    // Get the current action.
    ChannelAction *Action = Actions[Position];

    // Step the action.
    ChannelAction::ActionResult Result = Action->Step(Kind, *this);

    // Handle the result.
    switch (Result.Kind) {
    case ChannelAction::ActionResult::Retry:
      // We are done, we will restart with this action.
      return;

    case ChannelAction::ActionResult::Advance:
    case ChannelAction::ActionResult::Next:
      // Goto the next action, or restart if at the end. We increment the
      // position first because we want to reinitialize everything when we
      // restart.
      ++Position;
      GotoPosition(Position % Actions.size());

      // If this was an advance action, we are done.
      if (Result.Kind == ChannelAction::ActionResult::Advance)
        return;
      break;

    case ChannelAction::ActionResult::Goto:
      // Goto the requested position.
      GotoPosition(Result.Value);
      break;

    case ChannelAction::ActionResult::SwitchPrograms:
      // Ask the manager to switch programs at the next opportunity.
      GetProgram().GetManager().ChangePrograms();
      return;
    }
  }
}

/*
 * Channel Actions
 */

namespace {
  class SetLightAction : public ChannelAction {
    bool Enable;

  public:
    SetLightAction(bool Enable_) : Enable(Enable_) {}

    virtual ActionResult Step(MusicMonitorHandler::BeatKind Kind,
                              ChannelProgram &Program) {
      Program.SetLight(Enable);
      return ActionResult::MakeAdvance();
    }
  };

  class RepeatCount : public ChannelAction {
    unsigned Count;
    int GotoPosition;

    unsigned CurrentCount;

  public:
    RepeatCount(unsigned Count_, int GotoPosition_)
      : Count(Count_), GotoPosition(GotoPosition_) {}

    virtual void Initialize(ChannelProgram &Program) {
      CurrentCount = 1;
    }

    virtual ActionResult Step(MusicMonitorHandler::BeatKind Kind,
                              ChannelProgram &Program) {
      if (++CurrentCount < Count)
        return ActionResult::MakeGoto(Program.GetPosition() + GotoPosition);
      return ActionResult::MakeAdvance();
    }
  };

  class RepeatTime : public ChannelAction {
    double Time;
    int GotoPosition;

    double StartTime;

  public:
    RepeatTime(unsigned Time_, int GotoPosition_)
      : Time(Time_), GotoPosition(GotoPosition_) {}

    virtual void Initialize(ChannelProgram &Program) {
      StartTime = Program.GetProgram().GetActiveBeatElapsed();
    }

    virtual ActionResult Step(MusicMonitorHandler::BeatKind Kind,
                              ChannelProgram &Program) {
      double Elapsed = Program.GetProgram().GetActiveBeatElapsed() - StartTime;
      if (Elapsed < Time)
        return ActionResult::MakeGoto(Program.GetPosition() + GotoPosition);
      return ActionResult::MakeAdvance();
    }
  };

  class IfOkToStrobe : public ChannelAction {
    double Percent;
    int GotoPosition;
    double MinBPM;

  public:
    IfOkToStrobe(double Percent_, int GotoPosition_,
                 double MinBPM_ = 300.0)
      : Percent(Percent_), GotoPosition(GotoPosition_), MinBPM(MinBPM_) {}

    virtual ActionResult Step(MusicMonitorHandler::BeatKind Kind,
                              ChannelProgram &Program) {
      double BPM = Program.GetManager().GetRecentBPM();
      double Elapsed = get_elapsed_time_in_seconds();
      double EnabledTime = Program.GetLightState().TotalEnabledTime;
      double PercentStrobed =  EnabledTime / Elapsed;
      fprintf(stderr, "ok to strobe: %f / %f  = %f, bpm: %f < %f\n",
              EnabledTime, Elapsed, PercentStrobed, BPM, MinBPM);

      if (PercentStrobed <= Percent && BPM >= MinBPM)
        return ActionResult::MakeGoto(Program.GetPosition() + GotoPosition);
      return ActionResult::MakeAdvance();
    }
  };
}

///

static ChannelProgram *GetStrobeProgram() {
  ChannelProgram *P = new ChannelProgram(/*NeedStrobe=*/true);

  P->GetActions().push_back(new IfOkToStrobe(.1, 4));

  // Not ok to strobe.
  P->GetActions().push_back(new SetLightAction(false));
  P->GetActions().push_back(new RepeatCount(180, -1));
  P->GetActions().push_back(new RepeatCount(1, -3));

  // Ok to strobe.
  P->GetActions().push_back(new SetLightAction(true));
  P->GetActions().push_back(new RepeatCount(30, -1));
  P->GetActions().push_back(new SetLightAction(false));
  P->GetActions().push_back(new RepeatCount(30, -1));
  P->GetActions().push_back(new RepeatCount(5, -4));
                                           
  return P;
}

void LightProgram::LoadAllPrograms(std::vector<LightProgram *> &Result) {
  std::vector<ChannelProgram *> Programs;
  double MaxProgramTime = 60;
  ChannelProgram *P0, *P1, *P2;

  // Create a simple toggle program, by making alternating channels.
  P0 = new ChannelProgram();
  P0->GetActions().push_back(new SetLightAction(false));
  P0->GetActions().push_back(new SetLightAction(true));
  P1 = new ChannelProgram();
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new SetLightAction(false));

  Programs.clear();
  Programs.push_back(P0);
  Programs.push_back(P1);
  Programs.push_back(GetStrobeProgram());
  Result.push_back(new LightProgramImpl("alternating", MaxProgramTime,
                                        Programs));

  // Create a simple toggle program, by making alternating channels.
  P0 = new ChannelProgram();
  P0->GetActions().push_back(new SetLightAction(true));
  P1 = new ChannelProgram();
  P1->GetActions().push_back(new SetLightAction(true));
  P2 = new ChannelProgram();
  P2->GetActions().push_back(new SetLightAction(true));
  P2->GetActions().push_back(new SetLightAction(false));

  Programs.clear();
  Programs.push_back(P0);
  Programs.push_back(P1);
  Programs.push_back(P2);
  Programs.push_back(GetStrobeProgram());
  Result.push_back(new LightProgramImpl("alternating (2x)", MaxProgramTime,
                                        Programs));

  // Create a simple chase program.
  P0 = new ChannelProgram();
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new SetLightAction(false));
  P0->GetActions().push_back(new SetLightAction(false));
  P1 = new ChannelProgram();
  P1->GetActions().push_back(new SetLightAction(false));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new SetLightAction(false));
  P2 = new ChannelProgram();
  P2->GetActions().push_back(new SetLightAction(false));
  P2->GetActions().push_back(new SetLightAction(false));
  P2->GetActions().push_back(new SetLightAction(true));

  Programs.clear();
  Programs.push_back(P0);
  Programs.push_back(P1);
  Programs.push_back(P2);
  Programs.push_back(GetStrobeProgram());
  Result.push_back(new LightProgramImpl("chase", MaxProgramTime, Programs,
                                        /*ShortesteBeatInterval=*/.1));

  // Create a double chase program.
  P0 = new ChannelProgram();
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new SetLightAction(false));
  P1 = new ChannelProgram();
  P1->GetActions().push_back(new SetLightAction(false));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new SetLightAction(true));
  P2 = new ChannelProgram();
  P2->GetActions().push_back(new SetLightAction(true));
  P2->GetActions().push_back(new SetLightAction(false));
  P2->GetActions().push_back(new SetLightAction(true));

  Programs.clear();
  Programs.push_back(P0);
  Programs.push_back(P1);
  Programs.push_back(P2);
  Programs.push_back(GetStrobeProgram());
  Result.push_back(new LightProgramImpl("double chase", MaxProgramTime,
                                        Programs,
                                        /*ShortesteBeatInterval=*/.1));

  // Create a double chase (delayed) program.
  P0 = new ChannelProgram();
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new RepeatCount(4, -1));
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new RepeatCount(4, -1));
  P0->GetActions().push_back(new SetLightAction(false));
  P0->GetActions().push_back(new RepeatCount(4, -1));
  P1 = new ChannelProgram();
  P1->GetActions().push_back(new SetLightAction(false));
  P1->GetActions().push_back(new RepeatCount(4, -1));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new RepeatCount(4, -1));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new RepeatCount(4, -1));
  P2 = new ChannelProgram();
  P2->GetActions().push_back(new SetLightAction(true));
  P2->GetActions().push_back(new RepeatCount(4, -1));
  P2->GetActions().push_back(new SetLightAction(false));
  P2->GetActions().push_back(new RepeatCount(4, -1));
  P2->GetActions().push_back(new SetLightAction(true));
  P2->GetActions().push_back(new RepeatCount(4, -1));

  Programs.clear();
  Programs.push_back(P0);
  Programs.push_back(P1);
  Programs.push_back(P2);
  Result.push_back(new LightProgramImpl("double chase (slow)", MaxProgramTime,
                                        Programs,
                                        /*ShortesteBeatInterval=*/.1));

  // Create a slightly more complex toggle program, that leaves one light on
  // while toggling the other, then switches.
  P0 = new ChannelProgram();
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new SetLightAction(false));
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new SetLightAction(false));
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new SetLightAction(false));
  P1 = new ChannelProgram();
  P1->GetActions().push_back(new SetLightAction(false));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new SetLightAction(false));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new SetLightAction(false));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new SetLightAction(true));

  Programs.clear();
  Programs.push_back(P0);
  Programs.push_back(P1);
  Programs.push_back(GetStrobeProgram());
  Result.push_back(new LightProgramImpl("long alternating", MaxProgramTime,
                                        Programs));

  // Create an alternating toggle that toggles for 5s.
  P0 = new ChannelProgram();
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new RepeatTime(5, -1));
  P0->GetActions().push_back(new SetLightAction(false));
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new RepeatTime(10, -2));
  P1 = new ChannelProgram();
  P1->GetActions().push_back(new SetLightAction(false));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new RepeatTime(5, -2));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new RepeatTime(10, -1));

  Programs.clear();
  Programs.push_back(P0);
  Programs.push_back(P1);
  Programs.push_back(GetStrobeProgram());
  if (true)
    Result.push_back(new LightProgramImpl("timed alternating", MaxProgramTime,
                                          Programs));

  // Create a slow toggle that switches lights every 3s.
  P0 = new ChannelProgram();
  P0->GetActions().push_back(new SetLightAction(true));
  P0->GetActions().push_back(new RepeatTime(3, -1));
  P0->GetActions().push_back(new SetLightAction(false));
  P0->GetActions().push_back(new RepeatTime(6, -1));
  P1 = new ChannelProgram();
  P1->GetActions().push_back(new SetLightAction(false));
  P1->GetActions().push_back(new RepeatTime(3, -1));
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new RepeatTime(6, -1));

  Programs.clear();
  Programs.push_back(P0);
  Programs.push_back(P1);
  if (true)
    Result.push_back(new LightProgramImpl("slow alternating", MaxProgramTime,
                                          Programs));

  // Create a program that leaves one light on, and alternates the other one in
  // varying patterns.
  P0 = new ChannelProgram();
  P0->GetActions().push_back(new SetLightAction(false));
  P0->GetActions().push_back(new RepeatTime(1, -1));
  P0->GetActions().push_back(new SetLightAction(true));
  P1 = new ChannelProgram();
  P1->GetActions().push_back(new SetLightAction(true));

  Programs.clear();
  Programs.push_back(P0);
  Programs.push_back(P1);
  Result.push_back(new LightProgramImpl("stable with flicker", MaxProgramTime,
                                        Programs));

  // Very slow patterns (early).

  if (true) {
    P0 = new ChannelProgram();
    P0->GetActions().push_back(new SetLightAction(true));
    P1 = new ChannelProgram();
    P1->GetActions().push_back(new SetLightAction(false));
  
    Programs.clear();
    Programs.push_back(P0);
    Programs.push_back(P1);
    Result.push_back(new LightProgramImpl("static: mono", MaxProgramTime,
                                          Programs));
  }

  if (true) {
    P0 = new ChannelProgram();
    P0->GetActions().push_back(new SetLightAction(true));
    P1 = new ChannelProgram();
    P1->GetActions().push_back(new SetLightAction(true));
  
    Programs.clear();
    Programs.push_back(P0);
    Programs.push_back(P1);
    Result.push_back(new LightProgramImpl("static: dual", MaxProgramTime,
                                          Programs));
  }

  if (true) {
    unsigned Length = 90;
    P0 = new ChannelProgram();
    P0->GetActions().push_back(new SetLightAction(true));
    P0->GetActions().push_back(new RepeatCount(Length, -1));
    P0->GetActions().push_back(new SetLightAction(false));
    P0->GetActions().push_back(new RepeatCount(Length, -1));
    P1 = new ChannelProgram();
    P1->GetActions().push_back(new SetLightAction(false));
    P1->GetActions().push_back(new RepeatCount(Length, -1));
    P1->GetActions().push_back(new SetLightAction(true));
    P1->GetActions().push_back(new RepeatCount(Length, -1));
  
    Programs.clear();
    Programs.push_back(P0);
    Programs.push_back(P1);
    Result.push_back(new LightProgramImpl("vs: alternating", MaxProgramTime,
                                          Programs));
  }

  if (true) {
    unsigned Length = 90;
    P0 = new ChannelProgram();
    P0->GetActions().push_back(new SetLightAction(true));
    P0->GetActions().push_back(new RepeatCount(Length, -1));
    P0->GetActions().push_back(new SetLightAction(false));
    P0->GetActions().push_back(new RepeatCount(Length, -1));
    P0->GetActions().push_back(new SetLightAction(true));
    P0->GetActions().push_back(new RepeatCount(Length, -1));
    P1 = new ChannelProgram();
    P1->GetActions().push_back(new SetLightAction(false));
    P1->GetActions().push_back(new RepeatCount(Length, -1));
    P1->GetActions().push_back(new SetLightAction(true));
    P1->GetActions().push_back(new RepeatCount(Length, -1));
    P1->GetActions().push_back(new SetLightAction(true));
    P1->GetActions().push_back(new RepeatCount(Length, -1));
  
    Programs.clear();
    Programs.push_back(P0);
    Programs.push_back(P1);
    Result.push_back(new LightProgramImpl("vs: alternating (2)", MaxProgramTime,
                                          Programs));
  }
}
