{
  description = "Open Source Visual Servoing Platform";

  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:nim65s/nixpkgs/cxxheaderparser";
  };

  outputs =
    { flake-utils, nixpkgs, ... }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        visp = pkgs.callPackage ./default.nix { };
        py-visp = pkgs.python3Packages.toPythonModule (visp.override {
          pythonSupport = true;
        });
      in
      {
        packages = {
          inherit visp;
          default = py-visp;
        };
      }
    );
}
