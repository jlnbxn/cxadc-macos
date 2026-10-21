// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation
import SystemExtensions

func report(_ message: String) {
    FileHandle.standardError.write(Data((message + "\n").utf8))
}
final class Installer: NSObject, OSSystemExtensionRequestDelegate {
    func request(_ request: OSSystemExtensionRequest, actionForReplacingExtension existing: OSSystemExtensionProperties, withExtension ext: OSSystemExtensionProperties) -> OSSystemExtensionRequest.ReplacementAction {
        report("Replacing \(existing.bundleIdentifier) version \(existing.bundleVersion) with \(ext.bundleVersion)")
        return .replace
    }
    func requestNeedsUserApproval(_ request: OSSystemExtensionRequest) {
        report("Activation is waiting for user approval in System Settings → General → Login Items & Extensions → Driver Extensions.")
    }
    func request(_ request: OSSystemExtensionRequest, didFinishWithResult result: OSSystemExtensionRequest.Result) {
        report(result == .completed ? "System extension request completed. This does not prove that the PCI driver started; inspect CXADC logs." : "Request accepted; reboot required to complete it.")
        exit(result == .completed ? 0 : 3)
    }
    func request(_ request: OSSystemExtensionRequest, didFailWithError error: Error) {
        let e = error as NSError
        report("System extension request failed: \(e.domain) code=\(e.code): \(e.localizedDescription)")
        if e.code == OSSystemExtensionError.Code.unsupportedParentBundleLocation.rawValue {
            report("Install the host app in /Applications, or use Apple's documented system-extension developer mode for a local build.")
        }
        exit(1)
    }
}
let arguments = CommandLine.arguments
let operation = arguments.count == 2 ? arguments[1] : ""
guard operation == "activate" || operation == "deactivate" else {
    report("Usage: CXADCHost.app/Contents/MacOS/CXADCHost activate|deactivate")
    exit(64)
}
let identifier = "com.example.CXADC.Driver"
let delegate = Installer()
let request = operation == "activate"
    ? OSSystemExtensionRequest.activationRequest(forExtensionWithIdentifier: identifier, queue: .main)
    : OSSystemExtensionRequest.deactivationRequest(forExtensionWithIdentifier: identifier, queue: .main)
request.delegate = delegate
report("Submitting \(operation) request for \(identifier) from \(Bundle.main.bundlePath)")
OSSystemExtensionManager.shared.submitRequest(request)
DispatchQueue.main.asyncAfter(deadline: .now() + 45) {
    report("No completion within 45 seconds; request may be waiting for approval. Inspect System Settings and systemextensionsctl list.")
    exit(75)
}
dispatchMain()
