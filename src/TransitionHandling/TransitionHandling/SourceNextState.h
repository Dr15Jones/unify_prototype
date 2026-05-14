#ifndef TransitionHandling_SourceNextState_h
#define TransitionHandling_SourceNextState_h
/*
Enum to represent the next state of the source. This is used by the SourceCoordinator to determine which scheduler to run and which actions to take at the beginning and end of a transition.
*/
namespace edm {
  //NOTE: need to keep open the FileResource until each transition has finished processing.
  // There appears to be two kinds of dependencies between Transition resources
  // - phase dependent: the resource is only needed for begin and/or end transition processing. File dependencies are like this
  // - data dependent: the resource is needed for as long as the dependent transition's data are in use.
  enum class SourceNextState { File, DataTransition, Stop };

}  // namespace edm

#endif