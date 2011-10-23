#include "LightProgram.h"

#include "LightManager.h"
#include "Util.h"

#include <cassert>
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

    virtual void Initialize() {};

    virtual ActionResult Step(MusicMonitorHandler::BeatKind Kind,
                              ChannelProgram &Program) = 0;
  };

  class ChannelProgram {
    LightProgramImpl *ActiveProgram;
    unsigned ActiveLightIndex;

    std::vector<ChannelAction *> Actions;
    unsigned Position;

  public:
    ChannelProgram() : ActiveProgram(0), ActiveLightIndex(0), Position(0) {}

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
        Actions[i]->Initialize();
      Actions[0]->Start();
    }
    void Stop() {
      ActiveProgram = 0;
    }

    void Step(MusicMonitorHandler::BeatKind Kind);

    void GotoPosition(unsigned NewPosition) {
      assert(NewPosition < Actions.size() && "Invalid position!");

      // If we are jumping backwards, reinitialize all the actions in between.
      for (unsigned i = NewPosition + 1, e = Position + 1; i < e; ++i) {
        Actions[i]->Initialize();
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

  public:
    LightProgramImpl(std::string Name_, double MaxProgramTime_,
                     std::vector<ChannelProgram *> ChannelPrograms_)
      : ActiveManager(0),
        ActiveStartTime(-1),
        ActiveBeatElapsed(-1),
        Name(Name_),
        MaxProgramTime(MaxProgramTime_),
        ChannelPrograms(ChannelPrograms_)
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

      // FIXME: Create a random light assignment.
      ActiveAssignments.push_back(0);
      ActiveAssignments.push_back(1);

      for (unsigned i = 0, e = ChannelPrograms.size(); i != e; ++i) {
        ChannelPrograms[i]->Start(ActiveAssignments[i], *this);
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
      if (Elapsed - ActiveBeatElapsed < .01)
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
      // Goto the next action, or restart if at the end.
      GotoPosition((Position + 1) % Actions.size());

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
}

///

void LightProgram::LoadAllPrograms(std::vector<LightProgram *> &Result) {
  std::vector<ChannelProgram *> Programs;

  // Create a simple toggle program, by making alternating channels.
  ChannelProgram *P0 = new ChannelProgram();
  P0->GetActions().push_back(new SetLightAction(false));
  P0->GetActions().push_back(new SetLightAction(true));
  ChannelProgram *P1 = new ChannelProgram();
  P1->GetActions().push_back(new SetLightAction(true));
  P1->GetActions().push_back(new SetLightAction(false));

  Programs.clear();
  Programs.push_back(P0);
  Programs.push_back(P1);
  Result.push_back(new LightProgramImpl("alternating program", 180, Programs));

  // Create a slightly more complex toggle program, that leaves one light one
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
  Result.push_back(new LightProgramImpl("long alternating program", 180,
                                        Programs));
}


