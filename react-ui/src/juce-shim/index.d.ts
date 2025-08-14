declare namespace Juce {
  function getNativeFunction(name: string): (...args: any[]) => Promise<any>;
}
export = Juce;