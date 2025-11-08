// Actualizar el año en el footer
document.addEventListener("DOMContentLoaded", () => {
    // Funcionalidad original
    const currentYearElements = document.querySelectorAll("#current-year")
    const currentYear = new Date().getFullYear()
  
    currentYearElements.forEach((element) => {
      element.textContent = currentYear
    })

    // ===== NUEVAS FUNCIONES PARA AQUILES A1 REDISEÑADO =====

// Funcionalidad de video con scroll
document.addEventListener("DOMContentLoaded", () => {
  const scrollVideo = document.getElementById("scroll-video")
  const videoSection = document.querySelector(".video-scroll-section")
  const progressBar = document.querySelector(".progress-bar")
  const progressPercentage = document.getElementById("progress-percentage")
  const hotspots = document.querySelectorAll(".video-hotspot")

  if (scrollVideo && videoSection) {
    // Configurar el video
    scrollVideo.addEventListener("loadedmetadata", () => {
      // Video cargado
    })

    // Función para actualizar el video basado en el scroll
    function updateVideoOnScroll() {
      const rect = videoSection.getBoundingClientRect()
      const sectionHeight = videoSection.offsetHeight
      const windowHeight = window.innerHeight

      // Calcular el progreso del scroll dentro de la sección
      let scrollProgress = 0

      if (rect.top <= 0 && rect.bottom >= windowHeight) {
        // Estamos dentro de la sección
        scrollProgress = Math.abs(rect.top) / (sectionHeight - windowHeight)
        scrollProgress = Math.max(0, Math.min(1, scrollProgress))
      } else if (rect.bottom < windowHeight) {
        // Hemos pasado la sección
        scrollProgress = 1
      }

      // Actualizar el video
      if (scrollVideo.duration) {
        const targetTime = scrollProgress * scrollVideo.duration

        // Solo actualizar si hay una diferencia significativa
        if (Math.abs(scrollVideo.currentTime - targetTime) > 0.1) {
          scrollVideo.currentTime = targetTime
        }
      }

      // Actualizar la barra de progreso
      const percentage = Math.round(scrollProgress * 100)
      if (progressBar) {
        progressBar.style.setProperty("--progress", percentage + "%")
      }
      if (progressPercentage) {
        progressPercentage.textContent = percentage + "%"
      }

      // Actualizar hotspots
      updateHotspots(scrollProgress)
    }

    // Función para actualizar hotspots
    function updateHotspots(progress) {
      const progressPercent = progress * 100

      hotspots.forEach((hotspot) => {
        const trigger = Number.parseFloat(hotspot.getAttribute("data-trigger"))
        const tolerance = 5 // Tolerancia de 5%

        if (progressPercent >= trigger - tolerance && progressPercent <= trigger + tolerance + 10) {
          hotspot.classList.add("active")
        } else {
          hotspot.classList.remove("active")
        }
      })
    }

    // Event listener para scroll
    let ticking = false

    function onScroll() {
      if (!ticking) {
        requestAnimationFrame(() => {
          updateVideoOnScroll()
          ticking = false
        })
        ticking = true
      }
    }

    window.addEventListener("scroll", onScroll)

    // Llamar una vez al cargar
    updateVideoOnScroll()

    // Prevenir que el video se reproduzca automáticamente
    scrollVideo.addEventListener("play", (e) => {
      if (!scrollVideo.paused) {
        scrollVideo.pause()
      }
    })

    // Manejar errores de video
    scrollVideo.addEventListener("error", (e) => {
      // Error al cargar el video
    })
  }

  // Animaciones de entrada para las nuevas secciones
  const observerOptions = {
    threshold: 0.1,
    rootMargin: "0px 0px -50px 0px",
  }

  const observer = new IntersectionObserver((entries) => {
    entries.forEach((entry) => {
      if (entry.isIntersecting) {
        entry.target.classList.add("animate-in")

        // Animación especial para las tarjetas de especificaciones
        if (entry.target.classList.contains("spec-category-new")) {
          const delay = Array.from(entry.target.parentNode.children).indexOf(entry.target) * 100
          setTimeout(() => {
            entry.target.style.transform = "translateY(0)"
            entry.target.style.opacity = "1"
          }, delay)
        }

        // Animación especial para las tarjetas de beneficios
        if (entry.target.classList.contains("benefit-card-new")) {
          const delay = Array.from(entry.target.parentNode.children).indexOf(entry.target) * 150
          setTimeout(() => {
            entry.target.style.transform = "translateY(0)"
            entry.target.style.opacity = "1"
          }, delay)
        }
      }
    })
  }, observerOptions)

  // Observar elementos para animaciones
  document.querySelectorAll(".spec-category-new, .benefit-card-new").forEach((el) => {
    el.style.transform = "translateY(50px)"
    el.style.opacity = "0"
    el.style.transition = "all 0.6s cubic-bezier(0.175, 0.885, 0.32, 1.275)"
    observer.observe(el)
  })

  // Efecto parallax para el hero
  const heroParticles = document.querySelector(".hero-particles")
  if (heroParticles) {
    window.addEventListener("scroll", () => {
      const scrolled = window.pageYOffset
      const rate = scrolled * -0.5
      heroParticles.style.transform = `translateY(${rate}px)`
    })
  }

  // Animación de typing para el título del hero
  const titleLines = document.querySelectorAll(".title-line")
  titleLines.forEach((line, index) => {
    const text = line.textContent
    line.textContent = ""

    setTimeout(
      () => {
        let i = 0
        const typeInterval = setInterval(() => {
          line.textContent += text.charAt(i)
          i++
          if (i >= text.length) {
            clearInterval(typeInterval)
          }
        }, 100)
      },
      index * 500 + 500,
    )
  })

  // Efecto de hover 3D para las tarjetas
  document.querySelectorAll(".spec-category-new, .benefit-card-new").forEach((card) => {
    card.addEventListener("mousemove", (e) => {
      const rect = card.getBoundingClientRect()
      const x = e.clientX - rect.left
      const y = e.clientY - rect.top

      const centerX = rect.width / 2
      const centerY = rect.height / 2

      const rotateX = ((y - centerY) / centerY) * -10
      const rotateY = ((x - centerX) / centerX) * 10

      card.style.transform = `perspective(1000px) rotateX(${rotateX}deg) rotateY(${rotateY}deg) translateZ(10px)`
    })

    card.addEventListener("mouseleave", () => {
      card.style.transform = "perspective(1000px) rotateX(0deg) rotateY(0deg) translateZ(0px)"
    })
  })

  // Smooth scroll para los enlaces internos
  document.querySelectorAll('a[href^="#"]').forEach((anchor) => {
    anchor.addEventListener("click", function (e) {
      e.preventDefault()
      const target = document.querySelector(this.getAttribute("href"))
      if (target) {
        target.scrollIntoView({
          behavior: "smooth",
          block: "start",
        })
      }
    })
  })
  // ===== Award optional image toggles =====
  // Lightbox for award images (click image to open, outside or X to close)
  function createHeroParticles() {
    const heroBackground = document.querySelector(".hero-background")
    if (!heroBackground) return

    for (let i = 0; i < 50; i++) {
      const particle = document.createElement("div")
      particle.className = "floating-particle"
      particle.style.cssText = `
        position: absolute;
        width: ${Math.random() * 4 + 1}px;
        height: ${Math.random() * 4 + 1}px;
        background: rgba(140, 203, 213, ${Math.random() * 0.5 + 0.2});
        border-radius: 50%;
        top: ${Math.random() * 100}%;
        left: ${Math.random() * 100}%;
        animation: floatParticle ${Math.random() * 20 + 10}s linear infinite;
        animation-delay: ${Math.random() * 5}s;
      `
      heroBackground.appendChild(particle)
    }
  }

  // Agregar keyframes para las partículas flotantes
  const style = document.createElement("style")
  style.textContent = `
    @keyframes floatParticle {
      0% {
        transform: translateY(100vh) translateX(0);
        opacity: 0;
      }
      10% {
        opacity: 1;
      }
      90% {
        opacity: 1;
      }
      100% {
        transform: translateY(-100px) translateX(${Math.random() * 200 - 100}px);
        opacity: 0;
      }
    }
    
    .animate-in {
      animation: slideInUp 0.8s ease forwards;
    }
    
    @keyframes slideInUp {
      from {
        opacity: 0;
        transform: translateY(30px);
      }
      to {
        opacity: 1;
        transform: translateY(0);
      }
    }
  `
  document.head.appendChild(style)

  createHeroParticles()

  // Optimización de rendimiento para scroll
  let scrollTimeout
  window.addEventListener("scroll", () => {
    clearTimeout(scrollTimeout)
    scrollTimeout = setTimeout(() => {
      // Código que se ejecuta después de que el scroll se detiene
      document.body.classList.remove("scrolling")
    }, 150)

    document.body.classList.add("scrolling")
  })
})

// Función para manejar la carga del video
function handleVideoLoad() {
  const video = document.getElementById("scroll-video")
  if (video) {
    video.addEventListener("canplaythrough", () => {
      video.currentTime = 0
    })

    video.addEventListener("loadstart", () => {
      // Iniciando carga del video
    })

    video.addEventListener("progress", () => {
      if (video.buffered.length > 0) {
        const bufferedEnd = video.buffered.end(video.buffered.length - 1)
        const duration = video.duration
        if (duration > 0) {
          const bufferedPercent = (bufferedEnd / duration) * 100
          // Video cargado
        }
      }
    })
  }
}

// Llamar la función cuando el DOM esté listo
if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", handleVideoLoad)
} else {
  handleVideoLoad()
}

  
    // Menú móvil
    const mobileMenuBtn = document.querySelector(".mobile-menu-btn")
    const mobileMenu = document.querySelector(".mobile-menu")
  
    if (mobileMenuBtn && mobileMenu) {
      mobileMenuBtn.addEventListener("click", () => {
        mobileMenu.classList.toggle("active")
  
        // Cambiar el ícono del botón
        const icon = mobileMenuBtn.querySelector("i")
        if (mobileMenu.classList.contains("active")) {
          icon.classList.remove("fa-bars")
          icon.classList.add("fa-times")
        } else {
          icon.classList.remove("fa-times")
          icon.classList.add("fa-bars")
        }
      })

      // Función para cerrar el menú móvil
      function closeMobileMenu() {
        mobileMenu.classList.remove("active")
        const icon = mobileMenuBtn.querySelector("i")
        icon.classList.remove("fa-times")
        icon.classList.add("fa-bars")
      }

      // Cerrar el menú al hacer clic fuera de él
      document.addEventListener("click", (e) => {
        if (mobileMenu.classList.contains("active")) {
          // Verificar si el clic fue fuera del menú y del botón
          if (!mobileMenu.contains(e.target) && !mobileMenuBtn.contains(e.target)) {
            closeMobileMenu()
          }
        }
      })

      // Cerrar menú al detectar zoom/doble tap en mobile
      function installZoomCloseHandlers() {
        const vv = window.visualViewport
        let lastScale = vv ? vv.scale : 1

        function handlePotentialZoom() {
          if (mobileMenu.classList.contains("active")) {
            closeMobileMenu()
          }
        }

        // visualViewport: cambios de escala (pinch-to-zoom)
        if (vv) {
          vv.addEventListener("resize", () => {
            const newScale = vv.scale
            if (typeof newScale === "number" && Math.abs(newScale - lastScale) > 0.01) {
              lastScale = newScale
              handlePotentialZoom()
            }
          })
        }

        // iOS Safari: gestos de zoom
        window.addEventListener("gesturestart", handlePotentialZoom)
        window.addEventListener("gesturechange", handlePotentialZoom)
        window.addEventListener("gestureend", handlePotentialZoom)

        // Doble tap: detectar dos toques consecutivos en ~300ms en el mismo lugar
        let lastTapTime = 0
        let lastTapX = 0
        let lastTapY = 0
        document.addEventListener(
          "touchend",
          (e) => {
            const now = Date.now()
            const t = e.changedTouches && e.changedTouches[0]
            if (!t) return
            const dt = now - lastTapTime
            const dx = Math.abs(t.clientX - lastTapX)
            const dy = Math.abs(t.clientY - lastTapY)
            if (dt < 300 && dx < 20 && dy < 20) {
              handlePotentialZoom()
            }
            lastTapTime = now
            lastTapX = t.clientX
            lastTapY = t.clientY
          },
          { passive: true },
        )

        // Cambio de orientación
        window.addEventListener("orientationchange", handlePotentialZoom)
      }

      installZoomCloseHandlers()

      // Detectar zoom con diferentes métodos (compatibilidad adicional)
      window.addEventListener("resize", () => {
        // Si el viewport cambia por zoom y el menú está abierto, cerrar
        if (mobileMenu.classList.contains("active")) {
          closeMobileMenu()
        }
      })

      // Cerrar el menú al hacer scroll en mobile
      window.addEventListener(
        "scroll",
        () => {
          if (mobileMenu.classList.contains("active")) {
            closeMobileMenu()
          }
        },
        { passive: true },
      )
    }
  
    // Header con efecto de scroll
    const header = document.querySelector(".header")
  
    if (header) {
      window.addEventListener("scroll", () => {
        if (window.scrollY > 50) {
          header.classList.add("scrolled")
        } else {
          header.classList.remove("scrolled")
        }
      })
    }
  
    // Cerrar el menú móvil al hacer clic en un enlace
    const mobileMenuLinks = document.querySelectorAll(".mobile-menu a")
  
    mobileMenuLinks.forEach((link) => {
      link.addEventListener("click", () => {
        mobileMenu.classList.remove("active")
  
        const icon = mobileMenuBtn.querySelector("i")
        icon.classList.remove("fa-times")
        icon.classList.add("fa-bars")
      })
    })
  
    // Animación de entrada para elementos al hacer scroll
    const animateOnScroll = () => {
      const elements = document.querySelectorAll(".animate-on-scroll")
  
      elements.forEach((element) => {
        const elementPosition = element.getBoundingClientRect().top
        const screenPosition = window.innerHeight / 1.2
  
        if (elementPosition < screenPosition) {
          element.classList.add("animated")
        }
      })
    }
  
    // Aplicar clase para animación en scroll a elementos clave
    const applyAnimationClasses = () => {
      // Aplicar a elementos de la página principal
      document
        .querySelectorAll(
          ".feature-card, .service-card, .hero-text, .hero-image, .showcase-images, .showcase-text, .app-info, .app-mockup",
        )
        .forEach((el) => {
          el.classList.add("animate-on-scroll")
        })
  
      // Aplicar a elementos de otras páginas
      document
        .querySelectorAll(".about-card, .team-member, .gallery-item, .info-card, .values-card, .year-block, .cert-item")
        .forEach((el) => {
          el.classList.add("animate-on-scroll")
        })
    }
  
    applyAnimationClasses()
    animateOnScroll() // Ejecutar una vez al cargar
    window.addEventListener("scroll", animateOnScroll)
  
    // Contador animado para estadísticas
    const animateCounter = (element, target, duration) => {
      let start = 0
      const increment = target / (duration / 16)
  
      const updateCounter = () => {
        start += increment
        if (start < target) {
          element.textContent = Math.floor(start) + "%"
          requestAnimationFrame(updateCounter)
        } else {
          element.textContent = target + "%"
        }
      }
  
      updateCounter()
    }
  
    // Animar contadores cuando son visibles
    const animateCounters = () => {
      const counters = document.querySelectorAll(".stat-number")
  
      counters.forEach((counter) => {
        const elementPosition = counter.getBoundingClientRect().top
        const screenPosition = window.innerHeight / 1.2
  
        if (elementPosition < screenPosition && !counter.classList.contains("animated")) {
          counter.classList.add("animated")
          const target = Number.parseInt(counter.getAttribute("data-target"))
          animateCounter(counter, target, 2000)
        }
      })
    }
  
    window.addEventListener("scroll", animateCounters)
  
    // Crear partículas para el fondo
    const createParticles = () => {
      const heroSection = document.querySelector(".hero")
      if (heroSection && !document.getElementById("particles-js")) {
        const particlesContainer = document.createElement("div")
        particlesContainer.id = "particles-js"
        particlesContainer.style.position = "absolute"
        particlesContainer.style.top = "0"
        particlesContainer.style.left = "0"
        particlesContainer.style.width = "100%"
        particlesContainer.style.height = "100%"
        particlesContainer.style.zIndex = "1"
  
        heroSection.style.position = "relative"
        heroSection.style.overflow = "hidden"
  
        // Asegurarse de que el contenido esté por encima de las partículas
        const heroContainer = heroSection.querySelector(".container")
        if (heroContainer) {
          heroContainer.style.position = "relative"
          heroContainer.style.zIndex = "2"
        }
  
        heroSection.insertBefore(particlesContainer, heroSection.firstChild)
  
        // Cargar la librería particles.js si no está cargada
        if (typeof window.particlesJS === "undefined") {
          const script = document.createElement("script")
          script.src = "https://cdn.jsdelivr.net/particles.js/2.0.0/particles.min.js"
          script.onload = () => {
            initParticles()
          }
          document.head.appendChild(script)
        } else {
          initParticles()
        }
      }
    }
  
    const initParticles = () => {
      if (typeof window.particlesJS !== "undefined") {
        window.particlesJS("particles-js", {
          particles: {
            number: { value: 80, density: { enable: true, value_area: 800 } },
            color: { value: "#5cfffa" },
            shape: { type: "circle" },
            opacity: { value: 0.5, random: true },
            size: { value: 3, random: true },
            line_linked: {
              enable: true,
              distance: 150,
              color: "#5cfffa",
              opacity: 0.4,
              width: 1,
            },
            move: {
              enable: true,
              speed: 2,
              direction: "none",
              random: true,
              straight: false,
              out_mode: "out",
              bounce: false,
            },
          },
          interactivity: {
            detect_on: "canvas",
            events: {
              onhover: { enable: true, mode: "grab" },
              onclick: { enable: true, mode: "push" },
              resize: true,
            },
            modes: {
              grab: { distance: 140, line_linked: { opacity: 1 } },
              push: { particles_nb: 4 },
            },
          },
          retina_detect: true,
        })
      }
    }
  
    createParticles()
  
    // Crear partículas para la sección de la app
    const appParticles = document.getElementById("appParticles")
    if (appParticles) {
      // Crear partículas
      for (let i = 0; i < 50; i++) {
        const particle = document.createElement("div")
        particle.className = "particle"
        particle.style.position = "absolute"
        particle.style.width = Math.random() * 5 + "px"
        particle.style.height = particle.style.width
        particle.style.backgroundColor = "rgba(92, 255, 250, " + Math.random() * 0.3 + ")"
        particle.style.borderRadius = "50%"
        particle.style.top = Math.random() * 100 + "%"
        particle.style.left = Math.random() * 100 + "%"
        particle.style.animation = "float " + (Math.random() * 10 + 5) + "s linear infinite"
        particle.style.opacity = Math.random() * 0.5
  
        appParticles.appendChild(particle)
      }
    }
  
    // Crear líneas de circuito animadas
    const createCircuitLines = () => {
      const circuitContainers = document.querySelectorAll(".circuit-lines")
  
      circuitContainers.forEach((container) => {
        // Limpiar contenedor
        container.innerHTML = ""
  
        // Crear nuevas líneas
        for (let i = 0; i < 8; i++) {
          const line = document.createElement("div")
          line.className = "circuit-line"
  
          // Posición aleatoria
          line.style.top = Math.random() * 100 + "%"
          line.style.left = Math.random() * 50 + "%"
          line.style.width = Math.random() * 100 + 50 + "px"
  
          // Animación con retraso aleatorio
          line.style.animation = `glowPulse ${Math.random() * 2 + 2}s infinite alternate ${Math.random() * 2}s`
  
          container.appendChild(line)
        }
      })
    }
  
    createCircuitLines()
  
    // Efecto 3D para tarjetas
    const cards = document.querySelectorAll(".feature-card, .service-card")
  
    cards.forEach((card) => {
      card.addEventListener("mousemove", (e) => {
        const cardRect = card.getBoundingClientRect()
        const cardCenterX = cardRect.left + cardRect.width / 2
        const cardCenterY = cardRect.top + cardRect.height / 2
        const mouseX = e.clientX - cardCenterX
        const mouseY = e.clientY - cardCenterY
  
        // Calcular rotación basada en la posición del mouse
        const rotateX = (mouseY / (cardRect.height / 2)) * -5
        const rotateY = (mouseX / (cardRect.width / 2)) * 5
  
        card.style.transform = `perspective(1000px) rotateX(${rotateX}deg) rotateY(${rotateY}deg) scale(1.02)`
        card.style.transition = "transform 0.1s ease"
      })
  
      card.addEventListener("mouseleave", () => {
        card.style.transform = "perspective(1000px) rotateX(0) rotateY(0) scale(1)"
        card.style.transition = "transform 0.5s ease"
      })
    })
  
    // Efecto de typing para el título principal
    const heroTitle = document.querySelector(".hero-title span:first-child")
    if (heroTitle) {
      const text = heroTitle.textContent
      heroTitle.textContent = ""
      heroTitle.style.borderRight = "0.15em solid #5cfffa"
      heroTitle.style.animation = "blink-caret 0.75s step-end infinite"
  
      let i = 0
      const typeWriter = () => {
        if (i < text.length) {
          heroTitle.textContent += text.charAt(i)
          i++
          setTimeout(typeWriter, 100)
        } else {
          // Quitar el cursor después de terminar
          setTimeout(() => {
            heroTitle.style.borderRight = "none"
            heroTitle.style.animation = "none"
          }, 1500)
        }
      }
  
      setTimeout(typeWriter, 500)
    }
  })
  
  // Añadir keyframes para la animación de las partículas
  const style = document.createElement("style")
  style.innerHTML = `
  @keyframes float {
      0% {
          transform: translateY(0) translateX(0);
          opacity: 0;
      }
      50% {
          opacity: 0.5;
      }
      100% {
          transform: translateY(-100px) translateX(100px);
          opacity: 0;
      }
  }
  
  @keyframes blink-caret {
      from, to { border-color: transparent }
      50% { border-color: #5cfffa }
  }
  `
  
  document.head.appendChild(style)
  
  // Añadir al final del archivo script.js
  
  // Funcionalidad para los hotspots del exoesqueleto
  document.addEventListener("DOMContentLoaded", () => {
    // El panel IA ya está activo por defecto en el HTML para la vista back
  
      // Manejar clics en los hotspots
  const hotspots = document.querySelectorAll(".hotspot")
  hotspots.forEach((hotspot) => {
    hotspot.addEventListener("click", function () {
      const feature = this.getAttribute("data-feature")

      // Ocultar todos los paneles
      document.querySelectorAll(".spec-panel").forEach((panel) => {
        panel.classList.remove("active")
      })

      // Mostrar el panel correspondiente
      document.getElementById(`spec-${feature}`).classList.add("active")

      // Efecto visual en el hotspot
      this.querySelector(".hotspot-ring").style.animation = "none"
      setTimeout(() => {
        this.querySelector(".hotspot-ring").style.animation = "pulse 2s infinite"
      }, 10)

      // Ocultar el texto "Selecciona la parte!" cuando se selecciona cualquier hotspot
      const textHidden = document.querySelector(".text-hidden")
      if (textHidden) {
        textHidden.style.display = "none"
      }
    })
  })
  
    // Funcionalidad para las pestañas de especificaciones
    const specTabs = document.querySelectorAll(".spec-tab")
    specTabs.forEach((tab) => {
      tab.addEventListener("click", function () {
        const spec = this.getAttribute("data-spec")
  
        // Cambiar pestaña activa
        specTabs.forEach((t) => t.classList.remove("active"))
        this.classList.add("active")
  
        // Cambiar contenido activo
        document.querySelectorAll(".spec-content-panel").forEach((panel) => {
          panel.classList.remove("active")
        })
        document.getElementById(spec).classList.add("active")
      })
    })
  
  })


//Lightbox Function

function initLightbox() {
    const galleryItems = document.querySelectorAll('.gallery-image');
    const lightbox = document.createElement('div');
    lightbox.className = 'lightbox';
    lightbox.innerHTML = `
        <div class="lightbox-overlay"></div>
        <div class="lightbox-content">
            <img src="" alt="">
            <button class="lightbox-close">&times;</button>
            <button class="lightbox-prev">&lt;</button>
            <button class="lightbox-next">&gt;</button>
            <div class="lightbox-counter">1 / 1</div>
        </div>
    `;
    document.body.appendChild(lightbox);

    let currentIndex = 0;
    const images = Array.from(galleryItems).map(item => item.querySelector('img').src);

    galleryItems.forEach((item, index) => {
        item.addEventListener('click', () => {
            currentIndex = index;
            updateLightbox();
            lightbox.classList.add('active');
        });
    });

    function updateLightbox() {
        const lightboxImg = lightbox.querySelector('img');
        const counter = lightbox.querySelector('.lightbox-counter');
        lightboxImg.src = images[currentIndex];
        counter.textContent = `${currentIndex + 1} / ${images.length}`;
    }

    lightbox.querySelector('.lightbox-close').addEventListener('click', () => {
        lightbox.classList.remove('active');
    });

    lightbox.querySelector('.lightbox-prev').addEventListener('click', () => {
        currentIndex = (currentIndex - 1 + images.length) % images.length;
        updateLightbox();
    });

    lightbox.querySelector('.lightbox-next').addEventListener('click', () => {
        currentIndex = (currentIndex + 1) % images.length;
        updateLightbox();
    });

    // Close lightbox when clicking outside the image
    lightbox.addEventListener('click', (e) => {
        if (e.target === lightbox || e.target.classList.contains('lightbox-overlay')) {
            lightbox.classList.remove('active');
        }
    });

    // Keyboard navigation
    document.addEventListener('keydown', (e) => {
        if (!lightbox.classList.contains('active')) return;

        switch (e.key) {
            case 'Escape':
                lightbox.classList.remove('active');
                break;
            case 'ArrowLeft':
                currentIndex = (currentIndex - 1 + images.length) % images.length;
                updateLightbox();
                break;
            case 'ArrowRight':
                currentIndex = (currentIndex + 1) % images.length;
                updateLightbox();
                break;
        }
    });
}

// Initialize lightbox when DOM is loaded
document.addEventListener('DOMContentLoaded', initLightbox);

document.head.appendChild(style);
document.addEventListener('DOMContentLoaded', function() {
  var volverHotspot = document.getElementById('hotspot-volver');
  var exoImg = document.querySelector('.exo-main-image');
  var label = volverHotspot ? volverHotspot.querySelector('.hotspot-label') : null;

  // Paths (adjust if needed)
  var frontImg = "public/images/exoesqueleto-1.png";
  var backImg = "med/BACK.png";

  // Hotspot elements
  var hotspotIA = document.querySelector('.hotspot[data-feature="ia"]');
  var hotspotBateria = document.querySelector('.hotspot[data-feature="bateria"]');
  var hotspotSensor = document.querySelector('.hotspot[data-feature="sensor"]');
  var hotspotMotor = document.querySelector('.hotspot[data-feature="motor"]');
  var hotspotRegulador = document.querySelector('.hotspot[data-feature="regulador"]');


  // ========================================
  // SIST. De Posicionamiento
  // ========================================

  var imageBasedPositionsFront = {
      ia:        { top: "25%", left: "80%" },
      bateria:   { top: "8.5%", left: "71%" },
      sensor:    { top: "27%", left: "28%" },
      motor:     { top: "40%", left: "76%" },
      regulador: { top: "87.2%", left: "60%" },
      volver:    { top: "87%", left: "73%" }
  };
  
  var imageBasedPositionsBack = {
      ia:        { top: "24%", left: "34%" },
      bateria:   { top: "10%", left: "72%" },
      sensor:    { top: "60%", left: "20%" },
      motor:     { top: "33%", left: "19%" },
      regulador: { top: "87%", left: "35%" },
      volver:    { top: "87%", left: "73%" }
  };

  
  
  // Función para obtener las posiciones basadas en la imagen actual
  function getImageBasedPositions(isFrontView) {
    return isFrontView ? imageBasedPositionsFront : imageBasedPositionsBack;
  }
  
  // Función para aplicar posiciones basadas en la imagen
  function applyImageBasedPositions(positions) {
    // Obtener el contenedor de la imagen y la imagen misma
    const imageContainer = document.querySelector('.exo-image-container');
    const image = document.querySelector('.exo-main-image');
    
    if (!imageContainer || !image) {
      return;
    }
    
    // Obtener las dimensiones del contenedor y la imagen
    const containerRect = imageContainer.getBoundingClientRect();
    const imageRect = image.getBoundingClientRect();
    
    // Obtener las dimensiones CSS computadas de la imagen
    const computedStyle = window.getComputedStyle(image);
    const imageWidth = parseFloat(computedStyle.width);
    const imageHeight = parseFloat(computedStyle.height);
    
    // Calcular el offset de la imagen dentro del contenedor
    const imageOffsetX = imageRect.left - containerRect.left;
    const imageOffsetY = imageRect.top - containerRect.top;
    
    // Aplicar posiciones a cada hotspot
    Object.keys(positions).forEach(hotspotName => {
      let hotspot;
      
      // Manejo especial para el botón "volver" que no tiene data-feature
      if (hotspotName === 'volver') {
        hotspot = document.getElementById('hotspot-volver');
      } else {
        hotspot = document.querySelector(`.hotspot[data-feature="${hotspotName}"]`);
      }
      
      if (hotspot && positions[hotspotName]) {
          const position = positions[hotspotName];
          
          // Convertir porcentajes a píxeles basándose en las dimensiones CSS de la imagen
          let topPercent = parseFloat(position.top) / 100;
          let leftPercent = parseFloat(position.left) / 100;
          
          // Ajuste especial para el botón "volver" en pantallas pequeñas (0px a 500px)
          if (hotspotName === 'volver' && window.innerWidth <= 500) {
            leftPercent = 0.70; // 15% desde la izquierda de la imagen para pantallas pequeñas
            topPercent = 0.95; // 15% desde la izquierda de la imagen para pantallas pequeñas
          }
          if (hotspotName === 'volver' && window.innerWidth <= 420) {
            leftPercent = 0.70; // 15% desde la izquierda de la imagen para pantallas pequeñas
            topPercent = 0.95; // 15% desde la izquierda de la imagen para pantallas pequeñas
          }
          if (hotspotName === 'volver' && window.innerWidth <= 420) {
            leftPercent = 0.60; // 15% desde la izquierda de la imagen para pantallas pequeñas
            topPercent = 0.95; // 15% desde la izquierda de la imagen para pantallas pequeñas
          }
          
          // Calcular posición en píxeles relativos al contenedor
          // Usar las dimensiones CSS para un cálculo más preciso
          const topPixels = imageOffsetY + (imageHeight * topPercent);
          const leftPixels = imageOffsetX + (imageWidth * leftPercent);
          
          // Aplicar posición en píxeles
          hotspot.style.top = `${topPixels}px`;
          hotspot.style.left = `${leftPixels}px`;
        }
      }
    );
  }






  // Función para forzar actualización de posiciones
  window.forceUpdatePositions = function() {
    const isFrontView = isFront();
    const positions = getImageBasedPositions(isFrontView);
    setHotspotPositions(positions);
  };

  // Función actualizada para usar el sistema basado en imagen
  function setHotspotPositions(positions) {
      // Usar la nueva función de posicionamiento basado en imagen
      applyImageBasedPositions(positions);
  }

  // Helper to check which image is currently shown
  function isFront() {
      return exoImg.src.includes("exoesqueleto-1.png");
  }

  // Simple function to show/hide hotspots based on side
  function updateHotspotVisibility(side) {
      document.querySelectorAll('.hotspot').forEach(hotspot => {
          const hotspotSide = hotspot.getAttribute('data-side') || 'front';
          if (hotspotSide === side || hotspotSide === 'both') {
              hotspot.style.display = '';
          } else {
              hotspot.style.display = 'none';
          }
      });
  }

  // Y - coordenadas  de paneles desciptivos
  const specPanelTransformsFront = {
    'spec-motor': 'translateY(10%)',
    'spec-ia': 'translateY(10%)',
    'spec-bateria': 'translateY(10%)',
    'spec-sensor': 'translateY(10%)',
    'spec-regulador': 'translateY(10%)'
  };
  const specPanelTransformsBack = {
    'spec-ia': 'translateY(10%)',
    'spec-motor': 'translateY(10%)',
    'spec-bateria': 'translateY(10%)',
    'spec-sensor': 'translateY(10%)',
    'spec-regulador': 'translateY(10%)'
  };

  function setSpecPanelTransforms(transforms) {
    Object.keys(transforms).forEach(id => {
      const panel = document.getElementById(id);
      if (panel) {
        panel.style.transform = transforms[id];
      }
    });
  }

  
  let currentSide = 'back';

  // Helper to update positions on resize
  function updateHotspotPositionsOnResize() {
    const isFrontView = isFront();
    const positions = getImageBasedPositions(isFrontView);
    setHotspotPositions(positions);
  }

  // Listen for resize events
  window.addEventListener('resize', updateHotspotPositionsOnResize);

  if (volverHotspot && exoImg && label) {
      volverHotspot.addEventListener('click', function() {
          if (isFront()) {
              exoImg.src = backImg;
              const positions = getImageBasedPositions(false); // false = back view
              setHotspotPositions(positions);
              updateHotspotVisibility('back');
              setSpecPanelTransforms(specPanelTransformsBack);
              currentSide = 'back';
              // Show IA Adaptativa panel first on back
              document.querySelectorAll('.spec-panel').forEach(function(panel) {
                  panel.classList.remove('active');
              });
              var iaPanel = document.getElementById('spec-ia');
              if (iaPanel) iaPanel.classList.add('active');
          } else {
              exoImg.src = frontImg;
              const positions = getImageBasedPositions(true); // true = front view
              setHotspotPositions(positions);
              updateHotspotVisibility('front');
              setSpecPanelTransforms(specPanelTransformsFront);
              currentSide = 'front';
              // Show Modulo de Asistencia Motriz panel first on front
              document.querySelectorAll('.spec-panel').forEach(function(panel) {
                  panel.classList.remove('active');
              });
              var motorPanel = document.getElementById('spec-motor');
              if (motorPanel) motorPanel.classList.add('active');
          }
          label.innerHTML = '<i class="fa-solid fa-arrow-rotate-left"></i> Dar la vuelta';
      });

      // Set initial positions for back
      const initialPositions = getImageBasedPositions(false); // false = back view
      setHotspotPositions(initialPositions);
      updateHotspotVisibility('back');
      setSpecPanelTransforms(specPanelTransformsBack);
      currentSide = 'back';
      // Show Sistema de control adaptativo panel first on initial load
      document.querySelectorAll('.spec-panel').forEach(function(panel) {
          panel.classList.remove('active');
      });
      var iaPanel = document.getElementById('spec-ia');
      if (iaPanel) iaPanel.classList.add('active');
  }

  // Función mejorada para manejar cambios de tamaño
  function handleResponsiveChange() {
    // Con el nuevo sistema basado en imagen, necesitamos recalcular las posiciones
    const isFrontView = isFront();
    const positions = getImageBasedPositions(isFrontView);
    setHotspotPositions(positions);
  }

  // Listener para cambios de tamaño de ventana
  window.addEventListener('resize', handleResponsiveChange);
  
  // Listener para cuando la imagen cambie de tamaño (por ejemplo, al cambiar de vista)
  const image = document.querySelector('.exo-main-image');
  if (image) {
    image.addEventListener('load', function() {
      setTimeout(() => {
        const isFrontView = isFront();
        const positions = getImageBasedPositions(isFrontView);
        setHotspotPositions(positions);
      }, 100); // Pequeño delay para asegurar que la imagen esté completamente renderizada
    });
  }
});

// ===== MOBILE VISUAL CUE SYSTEM =====
function initMobileCue() {
  const mobileCue = document.getElementById('mobileCue');
  if (!mobileCue) return;

  let inactivityTimer;
  let secondTimer;
  let isCueVisible = false;
  let hasInteracted = false;
  let hasSeenTecnologiaAvanzada = false;

  // Function to check if user has seen "Tecnología Avanzada" section
  function checkIfSeenTecnologiaAvanzada() {
    const exoModel = document.querySelector('.exo-model');
    if (exoModel) {
      const rect = exoModel.getBoundingClientRect();
      const windowHeight = window.innerHeight;
      
      // Check if the exo-model is visible in the viewport
      if (rect.top < windowHeight && rect.bottom > 0) {
        hasSeenTecnologiaAvanzada = true;
        return true;
      }
    }
    return false;
  }

  // Function to show the cue
  function showCue() {
    if (!isCueVisible && !hasInteracted) {
      mobileCue.classList.add('show');
      isCueVisible = true;
    }
  }

  // Function to hide the cue
  function hideCue() {
    if (isCueVisible) {
      mobileCue.classList.remove('show');
      isCueVisible = false;
    }
  }

  // Function to reset inactivity timer
  function resetInactivityTimer() {
    clearTimeout(inactivityTimer);
    clearTimeout(secondTimer);
    
    // Hide cue if it's visible
    hideCue();
    
    // Check if user has seen the exo-model after interaction
    checkIfSeenTecnologiaAvanzada();
    
    // Set new timer for 5 seconds
    inactivityTimer = setTimeout(() => {
      if (!hasInteracted) {
        showCue();
        // Start second timer after first cue shows
        startSecondTimer();
      }
    }, 5000);
  }

  // Function to start second timer
  function startSecondTimer() {
    // Clear any existing second timer
    clearTimeout(secondTimer);
    
    // Start 3-second timer
    secondTimer = setTimeout(() => {
      // Check if user has seen the exo-model
      checkIfSeenTecnologiaAvanzada();
      if (!hasSeenTecnologiaAvanzada) {
        showCue();
      }
    }, 10000);
  }

  // Event listeners for user interaction
  const interactionEvents = [
    'touchstart',
    'touchmove', 
    'touchend',
    'scroll',
    'click',
    'mousemove'
  ];

  interactionEvents.forEach(event => {
    document.addEventListener(event, resetInactivityTimer, { passive: true });
  });

  // Start the initial timer
  inactivityTimer = setTimeout(() => {
    if (!hasInteracted) {
      showCue();
      // Start second timer after first cue shows
      startSecondTimer();
    }
  }, 5000);

  // Monitor scroll to check if user has seen "Tecnología Avanzada"
  let lastScrollTop = 0;
  window.addEventListener('scroll', () => {
    const scrollTop = window.pageYOffset || document.documentElement.scrollTop;
    
    // Check if user has seen the section
    checkIfSeenTecnologiaAvanzada();
    
    if (scrollTop > lastScrollTop + 100) { // Scrolled down more than 100px
      hideCue();
      hasInteracted = true;
    }
    lastScrollTop = scrollTop;
  }, { passive: true });
}

// Initialize mobile cue when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
  initMobileCue();
  initSponsorsCarousel();
  moveImageToTopOnMobile();
  
  // ===== Lightbox for award images =====
  const lightbox = document.createElement('div')
  lightbox.className = 'lightbox'
  lightbox.innerHTML = `
    <div class="lightbox-overlay"></div>
    <div class="lightbox-content">
      <img src="" alt="preview" />
      <div class="lightbox-close" aria-label="Cerrar">×</div>
    </div>
  `
  document.body.appendChild(lightbox)

  const lbImg = lightbox.querySelector('img')
  const lbClose = lightbox.querySelector('.lightbox-close')
  const lbOverlay = lightbox.querySelector('.lightbox-overlay')

  function openLightbox(src) {
    lbImg.src = src
    lightbox.classList.add('active')
  }
  function closeLightbox() {
    lightbox.classList.remove('active')
    lbImg.src = ''
  }
  lbClose.addEventListener('click', closeLightbox)
  lbOverlay.addEventListener('click', closeLightbox)
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') closeLightbox()
  })

  document.querySelectorAll('.award-image img').forEach((img) => {
    img.addEventListener('click', () => openLightbox(img.src))
  })
});

// ===== MOBILE IMAGE POSITIONING =====
function moveImageToTopOnMobile() {
  // Check if it's mobile view (width <= 768px)
  function isMobile() {
    return window.innerWidth <= 768;
  }

  // Function to hide image on mobile
  function hideImageOnMobile() {
    const mobileImage = document.querySelector('.mobile-hero-image');
    
    if (mobileImage && isMobile()) {
      // Hide the image completely on mobile
      mobileImage.style.display = 'none';
    }
  }

  // Function to show image on desktop
  function showImageOnDesktop() {
    const mobileImage = document.querySelector('.mobile-hero-image');
    
    if (mobileImage && !isMobile()) {
      // Show the image on desktop
      mobileImage.style.display = '';
    }
  }

  // Initial check
  if (isMobile()) {
    hideImageOnMobile();
  }

  // Handle window resize
  let resizeTimer;
  window.addEventListener('resize', () => {
    clearTimeout(resizeTimer);
    resizeTimer = setTimeout(() => {
      if (isMobile()) {
        hideImageOnMobile();
      } else {
        showImageOnDesktop();
      }
    }, 250);
  });
}

// ===== SPONSORS CAROUSEL FUNCTIONALITY =====
function initSponsorsCarousel() {
  const sponsorsTrack = document.getElementById('sponsorsTrack');
  if (!sponsorsTrack) return;

  let currentIndex = 0;
  const logos = sponsorsTrack.querySelectorAll('.sponsor-logo');
  const totalLogos = logos.length;
  const logoWidth = 120; // Width of each logo
  const gap = 60; // Gap between logos
  const moveDistance = logoWidth + gap;

  function moveToNext() {
    currentIndex = (currentIndex + 1) % (totalLogos / 2); // Only move through the first set of logos
    const translateX = -currentIndex * moveDistance;
    sponsorsTrack.style.transform = `translateX(${translateX}px)`;
  }

  // Move every 5 seconds
  setInterval(moveToNext, 5000);

  // Reset position when animation completes to create seamless loop
  sponsorsTrack.addEventListener('transitionend', () => {
    if (currentIndex >= totalLogos / 2) {
      currentIndex = 0;
      sponsorsTrack.style.transition = 'none';
      sponsorsTrack.style.transform = 'translateX(0)';
      setTimeout(() => {
        sponsorsTrack.style.transition = 'transform 0.5s ease-in-out';
      }, 10);
    }
  });
}

