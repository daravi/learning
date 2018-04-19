// Copyright 2016 Google Inc. All Rights Reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package main

import (
	"bytes"
	"crypto/ecdsa"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"fmt"
	"image"
	"image/draw"
	"image/png"
	"log"
	"math/big"
	"net"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/udacity/ud897-client-server-communication/utils"

	"github.com/GeertJohan/go.rice"
	"github.com/surma/httptools"
)

//go:generate rice embed-go
var box *rice.Box

func main() {
	box = rice.MustFindBox("assets")
	log.Printf("Website will be served on https://127.0.0.1.xip.io:8081")

	h1server := http.Server{
		Addr:         ":8080",
		ReadTimeout:  1 * time.Minute,
		WriteTimeout: 1 * time.Minute,
		Handler: httptools.List{
			httptools.SilentHandlerFunc(utils.NoCache),
			httptools.NewRegexpSwitch(map[string]http.Handler{
				"/tile": tileImageHandler(box, "logo.png"),
				"/.*":   http.FileServer(box.HTTPBox()),
			}),
		},
	}
	go func(h1server http.Server) {
		ln, err := net.Listen("tcp", h1server.Addr)
		if err != nil {
			log.Fatalf("Error opening socket: %s", err)
		}
		if err := h1server.Serve(ln); err != nil {
			log.Fatalf("Error starting webserver: %s", err)
		}
	}(h1server)

	h1server.Addr = ":8081"
	go func(h1server http.Server) {
		if err := configureTLS(&h1server); err != nil {
			log.Fatalf("Error configuring TLS: %s", err)
		}
		ln, err := net.Listen("tcp", h1server.Addr)
		if err != nil {
			log.Fatalf("Error opening socket: %s", err)
		}
		tcl := tls.NewListener(ln, h1server.TLSConfig)
		if err := h1server.Serve(tcl); err != nil {
			log.Fatalf("Error starting webserver: %s", err)
		}
	}(h1server)
	select {}
}

const (
	tileSize = 32
)

func tileImageHandler(b *rice.Box, fname string) http.Handler {
	encImg := b.MustBytes(fname)
	rawImg, _, err := image.Decode(bytes.NewReader(encImg))
	if err != nil {
		panic(err)
	}
	nimg := image.NewNRGBA(rawImg.Bounds())
	draw.Draw(nimg, nimg.Bounds(), rawImg, nimg.Bounds().Min, draw.Src)

	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		sX := r.FormValue("x")
		sY := r.FormValue("y")
		x, err := strconv.Atoi(sX)
		if err != nil {
			http.Error(w, http.StatusText(http.StatusBadRequest), http.StatusBadRequest)
			log.Printf("Invalid x coordinate: %s", err)
			return
		}
		y, err := strconv.Atoi(sY)
		if err != nil {
			http.Error(w, http.StatusText(http.StatusBadRequest), http.StatusBadRequest)
			log.Printf("Invalid y coordinate: %s", err)
			return
		}
		subImg := nimg.SubImage(image.Rect(x*tileSize, y*tileSize, (x+1)*tileSize, (y+1)*tileSize))
		w.Header().Set("Content-Type", "image/png")
		png.Encode(w, subImg)
	})
}

var (
	validFrom  = time.Now()
	validFor   = 365 * 24 * time.Hour
	isCA       = true
	rsaBits    = 2048
	ecdsaCurve = ""
)

func publicKey(priv interface{}) interface{} {
	switch k := priv.(type) {
	case *rsa.PrivateKey:
		return &k.PublicKey
	case *ecdsa.PrivateKey:
		return &k.PublicKey
	default:
		return nil
	}
}

func pemBlockForKey(priv interface{}) *pem.Block {
	switch k := priv.(type) {
	case *rsa.PrivateKey:
		return &pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(k)}
	case *ecdsa.PrivateKey:
		b, err := x509.MarshalECPrivateKey(k)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Unable to marshal ECDSA private key: %v", err)
			os.Exit(2)
		}
		return &pem.Block{Type: "EC PRIVATE KEY", Bytes: b}
	default:
		return nil
	}
}

func generateCertificates(host string) {
	var priv interface{}
	var err error
	priv, err = rsa.GenerateKey(rand.Reader, rsaBits)
	if err != nil {
		log.Fatalf("failed to generate private key: %s", err)
	}

	var notBefore = validFrom
	notAfter := notBefore.Add(validFor)

	serialNumberLimit := new(big.Int).Lsh(big.NewInt(1), 128)
	serialNumber, err := rand.Int(rand.Reader, serialNumberLimit)
	if err != nil {
		log.Fatalf("failed to generate serial number: %s", err)
	}

	template := x509.Certificate{
		SerialNumber: serialNumber,
		Subject: pkix.Name{
			Organization: []string{"Acme Co"},
		},
		NotBefore: notBefore,
		NotAfter:  notAfter,

		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
		BasicConstraintsValid: true,
	}

	hosts := strings.Split(host, ",")
	for _, h := range hosts {
		if ip := net.ParseIP(h); ip != nil {
			template.IPAddresses = append(template.IPAddresses, ip)
		} else {
			template.DNSNames = append(template.DNSNames, h)
		}
	}

	if isCA {
		template.IsCA = true
		template.KeyUsage |= x509.KeyUsageCertSign
	}

	derBytes, err := x509.CreateCertificate(rand.Reader, &template, &template, publicKey(priv), priv)
	if err != nil {
		log.Fatalf("Failed to create certificate: %s", err)
	}

	certOut, err := os.Create("cert.pem")
	if err != nil {
		log.Fatalf("failed to open cert.pem for writing: %s", err)
	}
	pem.Encode(certOut, &pem.Block{Type: "CERTIFICATE", Bytes: derBytes})
	certOut.Close()
	log.Print("written cert.pem\n")

	keyOut, err := os.OpenFile("key.pem", os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0600)
	if err != nil {
		log.Print("failed to open key.pem for writing:", err)
		return
	}
	pem.Encode(keyOut, pemBlockForKey(priv))
	keyOut.Close()
	log.Print("written key.pem\n")
}

func configureTLS(server *http.Server) error {
	if _, err := os.Stat("cert.pem"); err != nil {
		log.Printf("Generating certificate...")
		generateCertificates("127.0.0.1.xip.io")
	}

	cert, err := tls.LoadX509KeyPair("cert.pem", "key.pem")
	if err != nil {
		return err
	}

	if server.TLSConfig == nil {
		server.TLSConfig = &tls.Config{}
	}
	server.TLSConfig.PreferServerCipherSuites = true
	server.TLSConfig.NextProtos = append(server.TLSConfig.NextProtos, "http/1.1")
	server.TLSConfig.Certificates = []tls.Certificate{cert}
	return nil
}
