class Slermes < Formula
  desc "C translation of Hermes Agent by Nous Research"
  homepage "https://github.com/waefrebeorn/slermes"
  url "https://github.com/waefrebeorn/slermes/archive/refs/tags/v0.15.1.tar.gz"
  sha256 ""
  license "MIT"
  head "https://github.com/waefrebeorn/slermes.git", branch: "main"

  depends_on "pkg-config" => :build
  depends_on "openssl"
  depends_on "zlib"

  def install
    system "make", "slermes"
    bin.install "slermes"
  end

  test do
    assert_match "Slermes", shell_output("#{bin}/slermes --version 2>&1", 1)
  end
end
