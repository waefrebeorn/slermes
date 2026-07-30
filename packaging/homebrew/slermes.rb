# Homebrew Formula for Slermes
# Install: brew install --formula packaging/homebrew/slermes.rb

class Slermes < Formula
  desc "C Language Hermes Agent — self-improving AI assistant"
  homepage "https://github.com/NousResearch/hermes-agent"
  url "https://github.com/waefrebeorn/slermes/archive/refs/tags/v502.tar.gz"
  sha256 "" # TODO: update after release
  license "MIT"
  head "https://github.com/waefrebeorn/slermes.git", branch: "main"

  depends_on "openssl@3"
  depends_on "pkg-config" => :build
  depends_on "gcc" => :build

  def install
    system "make", "-j#{ENV.make_jobs}"
    bin.install "slermes"
    doc.install "README.md"
    doc.install "LICENSE" if File.exist?("LICENSE")
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/slermes --version")
    assert_match /slermes/i, shell_output("#{bin}/slermes --help")
  end
end
