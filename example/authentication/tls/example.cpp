#include <iostream>
#include <fstream>
#include "VaultClient.h"
#include "../../shared/shared.h"

std::optional<std::string> enableTls(const Vault::Sys::Auth &authAdmin) {
  return authAdmin.enable(Vault::Path{"cert"}, Vault::Parameters{{"type", "cert"}});
}

std::optional<std::string> setupTlsAuthentication(const Vault::Tls &tls, const Vault::Certificate &certificate) {
  Vault::Parameters parameters({
    {"display_name", "example"},
    {"certificate", certificate.value()}
  });

  return tls.createRole(Vault::Path{"example"}, parameters);
}

std::optional<std::string> deleteRole(const Vault::Pki &pkiAdmin) {
  return pkiAdmin.deleteRole(Vault::Path{"example"});
}

std::optional<std::string> disableTls(const Vault::Sys::Auth &authAdmin) {
  return authAdmin.disable(Vault::Path{"cert"});
}

std::optional<std::string> generateRootCertificate(const Vault::Pki &pki) {
  Vault::Parameters rootCertificateParameters({
    {"common_name", "my-website.com"},
    {"ttl",         "8760h"}
  });

  return pki.generateRoot(Vault::RootCertificateTypes::INTERNAL, rootCertificateParameters);
}

std::optional<std::string> setUrlParameters(const Vault::Pki &pki) {
  Vault::Parameters urlParameters({
    {"issuing_certificates",    "http://127.0.0.1:8200/v1/pki/ca"},
    {"crl_distribution_points", "http://127.0.0.1:8200/v1/pki/crl"}
  });

  return pki.setUrls(urlParameters);
}

std::optional<std::string> createRole(const Vault::Pki &pki) {
  Vault::Parameters roleParameters({
    {"allowed_domains",  "my-website.com"},
    {"allow_subdomains", "true"},
    {"max_ttl",          "72h"}
  });

  return pki.createRole(Vault::Path{"example-dot-com"}, roleParameters);
}

std::optional<std::string> issueCertificate(const Vault::Pki &pki) {
  Vault::Path path("example-dot-com");
  Vault::Parameters parameters({{"common_name", "www.my-website.com"}});

  return pki.issue(path, parameters);
}

std::optional<std::string> enablePki(const Vault::Sys::Mounts &mountAdmin) {
  return mountAdmin.enable(Vault::Path{"pki"}, Vault::Parameters{{"type", "pki"}});
}

Vault::Certificate setupPki(const Vault::Client &rootClient, const Vault::Sys::Mounts &mountAdmin) {
  enablePki(mountAdmin);

  Vault::Pki pki{rootClient};
  generateRootCertificate(pki);
  setUrlParameters(pki);
  createRole(pki);

  auto certificateResponse = issueCertificate(pki);
  if (certificateResponse) {
    Vault::Certificate cert{nlohmann::json::parse(certificateResponse.value())["data"]["certificate"]};
    Vault::Certificate key{nlohmann::json::parse(certificateResponse.value())["data"]["private_key"]};
    std::ofstream certFile;
    certFile.open("cert.pem");
    certFile << cert.value() << std::endl;
    certFile.close();
    std::ofstream keyFile;
    keyFile.open("key.pem");
    keyFile << key.value() << std::endl;
    keyFile.close();
    return cert;
  } else {
    std::cout << "Unable to generate client certificate" << std::endl;
    exit(-1);
  }
}

std::optional<std::string> disablePki(const Vault::Sys::Mounts &mountAdmin) {
  return mountAdmin.disable(Vault::Path{"pki"});
}

Vault::Client setup(const Vault::Client &rootClient) {
  Vault::Sys::Mounts mountAdmin{rootClient};
  Vault::Sys::Auth authAdmin{rootClient};
  enablePki(mountAdmin);
  Vault::Certificate cert = setupPki(rootClient, mountAdmin);
  enableTls(authAdmin);
  Vault::Tls tls{rootClient};
  setupTlsAuthentication(tls, cert);

  Vault::TlsStrategy tlsStrategy{Vault::Certificate{"cert.pem"}, Vault::Certificate{"key.pem"}};
  Vault::Config config = Vault::ConfigBuilder().withDebug(true).withTlsVerification(false).withTlsEnabled(false).build();
  Vault::HttpErrorCallback httpErrorCallback = [&](std::string err) {
    std::cout << err << std::endl;
  };

  return Vault::Client{config, tlsStrategy, httpErrorCallback};
}

void cleanup(const Vault::Client &rootClient) {
  Vault::Sys::Auth authAdmin = Vault::Sys::Auth{rootClient};
  Vault::Sys::Mounts mountAdmin{rootClient};
  Vault::Pki pkiAdmin{rootClient};

  deleteRole(pkiAdmin);
  disablePki(mountAdmin);
  disableTls(authAdmin);
}

int main(void) {
  char *rootTokenEnv = std::getenv("VAULT_ROOT_TOKEN");
  if (!rootTokenEnv) {
    std::cout << "The VAULT_ROOT_TOKEN environment variable must be set" << std::endl;
    exit(-1);
  }
  Vault::Token rootToken{rootTokenEnv};
  Vault::Client rootClient = getRootClient(rootToken);
  Vault::Client tlsClient = setup(rootClient);

  if (tlsClient.is_authenticated()) {
    std::cout << "Authenticated: " << tlsClient.getToken() << std::endl;
  } else {
    std::cout << "Unable to authenticate" << std::endl;
  }

  //cleanup(rootClient);
}
