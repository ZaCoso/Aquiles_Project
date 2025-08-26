// Actualizar el año en el footer
document.addEventListener('DOMContentLoaded', function() {
    // Funcionalidad original
    const currentYearElements = document.querySelectorAll('#current-year');
    const currentYear = new Date().getFullYear();
    
    currentYearElements.forEach(element => {
        element.textContent = currentYear;
    });
    
    // Menú móvil
    const mobileMenuBtn = document.querySelector('.mobile-menu-btn');
    const mobileMenu = document.querySelector('.mobile-menu');
    
    if (mobileMenuBtn && mobileMenu) {
        mobileMenuBtn.addEventListener('click', function() {
            mobileMenu.classList.toggle('active');
            
            // Cambiar el ícono del botón
            const icon = mobileMenuBtn.querySelector('i');
            if (mobileMenu.classList.contains('active')) {
                icon.classList.remove('fa-bars');
                icon.classList.add('fa-times');
            } else {
                icon.classList.remove('fa-times');
                icon.classList.add('fa-bars');
            }
        });
    }
    
    // Header con efecto de scroll
    const header = document.querySelector('.header');
    
    if (header) {
        window.addEventListener('scroll', function() {
            if (window.scrollY > 50) {
                header.classList.add('scrolled');
            } else {
                header.classList.remove('scrolled');
            }
        });
    }
    
    // Cerrar el menú móvil al hacer clic en un enlace
    const mobileMenuLinks = document.querySelectorAll('.mobile-menu a');
    
    mobileMenuLinks.forEach(link => {
        link.addEventListener('click', function() {
            mobileMenu.classList.remove('active');
            
            const icon = mobileMenuBtn.querySelector('i');
            icon.classList.remove('fa-times');
            icon.classList.add('fa-bars');
        });
    });

    // NUEVAS ANIMACIONES Y EFECTOS

    // 1. Animación de entrada para elementos al hacer scroll
    const animateOnScroll = function() {
        const elements = document.querySelectorAll('.animate-on-scroll');
        
        elements.forEach(element => {
            const elementPosition = element.getBoundingClientRect().top;
            const screenPosition = window.innerHeight / 1.2;
            
            if (elementPosition < screenPosition) {
                element.classList.add('animated');
            }
        });
    };
    
    // Aplicar clase para animación en scroll a elementos clave
    const applyAnimationClasses = function() {
        // Aplicar a elementos de la página principal
        document.querySelectorAll('.feature-card, .service-card, .hero h1, .hero p').forEach(el => {
            el.classList.add('animate-on-scroll');
        });
        
        // Aplicar a elementos de otras páginas
        document.querySelectorAll('.about-card, .team-member, .gallery-item, .info-card, .values-card, .year-block, .cert-item').forEach(el => {
            el.classList.add('animate-on-scroll');
        });
    };
    
    applyAnimationClasses();
    animateOnScroll(); // Ejecutar una vez al cargar
    window.addEventListener('scroll', animateOnScroll);
    
    // 2. Efecto de parallax para el fondo
    const parallaxBg = document.querySelector('body');
    if (parallaxBg) {
        window.addEventListener('scroll', function() {
            const scrollPosition = window.pageYOffset;
            parallaxBg.style.backgroundPositionY = scrollPosition * 0.5 + 'px';
        });
    }
    
    // 3. Efecto hover 3D para tarjetas
    const cards = document.querySelectorAll('.feature-card, .service-card, .team-member, .gallery-item, .info-card');
    
    cards.forEach(card => {
        card.addEventListener('mousemove', function(e) {
            const cardRect = card.getBoundingClientRect();
            const cardCenterX = cardRect.left + cardRect.width / 2;
            const cardCenterY = cardRect.top + cardRect.height / 2;
            const mouseX = e.clientX - cardCenterX;
            const mouseY = e.clientY - cardCenterY;
            
            // Calcular rotación basada en la posición del mouse
            const rotateX = (mouseY / (cardRect.height / 2)) * -5;
            const rotateY = (mouseX / (cardRect.width / 2)) * 5;
            
            card.style.transform = `perspective(1000px) rotateX(${rotateX}deg) rotateY(${rotateY}deg) scale(1.02)`;
            card.style.transition = 'transform 0.1s ease';
        });
        
        card.addEventListener('mouseleave', function() {
            card.style.transform = 'perspective(1000px) rotateX(0) rotateY(0) scale(1)';
            card.style.transition = 'transform 0.5s ease';
        });
    });
    
    // 4. Efecto de typing para el título principal
    const heroTitle = document.querySelector('.project-title');
    if (heroTitle) {
        const text = heroTitle.textContent;
        heroTitle.textContent = '';
        heroTitle.style.borderRight = '0.15em solid #8CCBD5';
        heroTitle.style.animation = 'blink-caret 0.75s step-end infinite';
        
        let i = 0;
        const typeWriter = function() {
            if (i < text.length) {
                heroTitle.textContent += text.charAt(i);
                i++;
                setTimeout(typeWriter, 100);
            } else {
                // Quitar el cursor después de terminar
                setTimeout(() => {
                    heroTitle.style.borderRight = 'none';
                    heroTitle.style.animation = 'none';
                }, 1500);
            }
        };
        
        setTimeout(typeWriter, 500);
    }
    
    // 5. Contador animado para estadísticas (MODIFICADO PARA MOSTRAR %)
    const animateCounter = function(element, target, duration) {
        let start = 0;
        const increment = target / (duration / 16);
        
        const updateCounter = function() {
            start += increment;
            if (start < target) {
                element.textContent = Math.floor(start) + '%';
                requestAnimationFrame(updateCounter);
            } else {
                element.textContent = target + '%';
            }
        };
        
        updateCounter();
    };
    
    // Crear sección de estadísticas si no existe
    const createStatsSection = function() {
        const main = document.querySelector('main');
        if (main && document.querySelector('.hero') && !document.querySelector('.stats-section')) {
            const statsSection = document.createElement('section');
            statsSection.className = 'stats-section';
            statsSection.innerHTML = `
                <div class="container">
                    <div class="stats-grid">
                        <div class="stat-item">
                            <div class="stat-number" data-target="100">0%</div>
                            <div class="stat-label">Eficiencia</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-number" data-target="100">0%</div>
                            <div class="stat-label">Innovador</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-number" data-target="100">0%</div>
                            <div class="stat-label">Ergonómico</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-number" data-target="100">0%</div>
                            <div class="stat-label">Adaptable</div>
                        </div>
                    </div>
                </div>
            `;
            
            // Insertar después de la sección hero
            const heroSection = document.querySelector('.hero');
            if (heroSection && heroSection.nextElementSibling) {
                main.insertBefore(statsSection, heroSection.nextElementSibling);
            } else if (heroSection) {
                main.appendChild(statsSection);
            }
        }
    };
    
    createStatsSection();
    
    // Animar contadores cuando son visibles
    const animateCounters = function() {
        const counters = document.querySelectorAll('.stat-number');
        
        counters.forEach(counter => {
            const elementPosition = counter.getBoundingClientRect().top;
            const screenPosition = window.innerHeight / 1.2;
            
            if (elementPosition < screenPosition && !counter.classList.contains('animated')) {
                counter.classList.add('animated');
                const target = parseInt(counter.getAttribute('data-target'));
                animateCounter(counter, target, 2000);
            }
        });
    };
    
    window.addEventListener('scroll', animateCounters);
    
    // 6. Efecto de partículas en el fondo para la página principal
    const createParticles = function() {
        const heroSection = document.querySelector('.hero');
        if (heroSection && !document.getElementById('particles-js')) {
            const particlesContainer = document.createElement('div');
            particlesContainer.id = 'particles-js';
            particlesContainer.style.position = 'absolute';
            particlesContainer.style.top = '0';
            particlesContainer.style.left = '0';
            particlesContainer.style.width = '100%';
            particlesContainer.style.height = '100%';
            particlesContainer.style.zIndex = '1';
            
            heroSection.style.position = 'relative';
            heroSection.style.overflow = 'hidden';
            
            // Asegurarse de que el contenido esté por encima de las partículas
            const heroContainer = heroSection.querySelector('.container');
            if (heroContainer) {
                heroContainer.style.position = 'relative';
                heroContainer.style.zIndex = '2';
            }
            
            heroSection.insertBefore(particlesContainer, heroSection.firstChild);
            
            // Cargar la librería particles.js si no está cargada
            if (typeof particlesJS === 'undefined') {
                const script = document.createElement('script');
                script.src = 'https://cdn.jsdelivr.net/particles.js/2.0.0/particles.min.js';
                script.onload = function() {
                    initParticles();
                };
                document.head.appendChild(script);
            } else {
                initParticles();
            }
        }
    };
    
    const initParticles = function() {
        if (typeof particlesJS !== 'undefined') {
            particlesJS('particles-js', {
                particles: {
                    number: { value: 80, density: { enable: true, value_area: 800 } },
                    color: { value: '#8CCBD5' },
                    shape: { type: 'circle' },
                    opacity: { value: 0.5, random: false },
                    size: { value: 3, random: true },
                    line_linked: {
                        enable: true,
                        distance: 150,
                        color: '#8CCBD5',
                        opacity: 0.4,
                        width: 1
                    },
                    move: {
                        enable: true,
                        speed: 2,
                        direction: 'none',
                        random: false,
                        straight: false,
                        out_mode: 'out',
                        bounce: false
                    }
                },
                interactivity: {
                    detect_on: 'canvas',
                    events: {
                        onhover: { enable: true, mode: 'grab' },
                        onclick: { enable: true, mode: 'push' },
                        resize: true
                    },
                    modes: {
                        grab: { distance: 140, line_linked: { opacity: 1 } },
                        push: { particles_nb: 4 }
                    }
                },
                retina_detect: true
            });
        }
    };
    
    createParticles();
    
    // 7. Añadir efecto de desplazamiento suave para los enlaces internos
    document.querySelectorAll('a[href^="#"]').forEach(anchor => {
        anchor.addEventListener('click', function(e) {
            e.preventDefault();
            
            const targetId = this.getAttribute('href');
            if (targetId === '#') return;
            
            const targetElement = document.querySelector(targetId);
            if (targetElement) {
                window.scrollTo({
                    top: targetElement.offsetTop - 100,
                    behavior: 'smooth'
                });
            }
        });
    });
    
    // 8. Añadir efecto de zoom en hover para imágenes de la galería
    document.querySelectorAll('.gallery-image img').forEach(img => {
        img.addEventListener('mouseover', function() {
            this.style.transform = 'scale(1.1)';
            this.style.transition = 'transform 0.5s ease';
        });
        
        img.addEventListener('mouseout', function() {
            this.style.transform = 'scale(1)';
        });
    });
    
    // 9. Añadir efecto de revelación para el formulario de contacto
    const contactForm = document.querySelector('.form-container');
    if (contactForm) {
        const formGroups = contactForm.querySelectorAll('.form-group');
        formGroups.forEach((group, index) => {
            group.style.opacity = '0';
            group.style.transform = 'translateY(20px)';
            group.style.transition = 'opacity 0.5s ease, transform 0.5s ease';
            
            setTimeout(() => {
                group.style.opacity = '1';
                group.style.transform = 'translateY(0)';
            }, 300 + (index * 200));
        });
    }
    
    // 10. Animación para la sección de la aplicación móvil
    const appSection = document.querySelector('.app-showcase');
    if (appSection) {
        // Animación para el teléfono
        const phoneFrame = document.querySelector('.phone-frame');
        if (phoneFrame) {
            window.addEventListener('scroll', function() {
                const phonePosition = phoneFrame.getBoundingClientRect().top;
                const screenPosition = window.innerHeight / 1.2;
                
                if (phonePosition < screenPosition) {
                    phoneFrame.style.transform = 'rotate(0deg) perspective(800px) rotateY(0deg)';
                    phoneFrame.style.opacity = '1';
                }
            });
        }
        
        // Animación para el slider de potencia
        const sliderHandle = document.querySelector('.slider-handle');
        const sliderFill = document.querySelector('.slider-fill');
        const sliderValue = document.querySelector('.slider-value');
        
        if (sliderHandle && sliderFill && sliderValue) {
            let isDragging = false;
            
            sliderHandle.addEventListener('mousedown', function(e) {
                isDragging = true;
                e.preventDefault();
            });
            
            document.addEventListener('mouseup', function() {
                isDragging = false;
            });
            
            document.addEventListener('mousemove', function(e) {
                if (isDragging) {
                    const sliderTrack = document.querySelector('.slider-track');
                    const trackRect = sliderTrack.getBoundingClientRect();
                    let position = (e.clientX - trackRect.left) / trackRect.width;
                    
                    // Limitar entre 0 y 1
                    position = Math.max(0, Math.min(1, position));
                    
                    // Actualizar posición del handle y fill
                    sliderHandle.style.left = position * 100 + '%';
                    sliderFill.style.width = position * 100 + '%';
                    
                    // Actualizar valor
                    const value = Math.round(position * 100);
                    sliderValue.textContent = value + '%';
                }
            });
        }
        
        // Animación para los modos
        const modes = document.querySelectorAll('.mode');
        if (modes.length > 0) {
            modes.forEach(mode => {
                mode.addEventListener('click', function() {
                    // Quitar clase activa de todos los modos
                    modes.forEach(m => m.classList.remove('active'));
                    
                    // Añadir clase activa al modo clickeado
                    this.classList.add('active');
                });
            });
        }
        
        // Crear partículas para la sección de la app
        const appParticles = document.getElementById('appParticles');
        if (appParticles) {
            // Crear partículas
            for (let i = 0; i < 50; i++) {
                const particle = document.createElement('div');
                particle.className = 'particle';
                particle.style.position = 'absolute';
                particle.style.width = Math.random() * 5 + 'px';
                particle.style.height = particle.style.width;
                particle.style.backgroundColor = 'rgba(255, 255, 255, ' + (Math.random() * 0.3) + ')';
                particle.style.borderRadius = '50%';
                particle.style.top = Math.random() * 100 + '%';
                particle.style.left = Math.random() * 100 + '%';
                particle.style.animation = 'float ' + (Math.random() * 10 + 5) + 's linear infinite';
                particle.style.opacity = Math.random() * 0.5;
                
                appParticles.appendChild(particle);
            }
        }
    }
});

// Añadir keyframes para la animación de las partículas
const style = document.createElement('style');
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
    50% { border-color: #8CCBD5 }
}
`;
// Animación para la sección de la aplicación móvil
const appSection = document.querySelector('.app-showcase');
if (appSection) {
    // Animación para el teléfono
    const phoneFrame = document.querySelector('.phone-frame');
    if (phoneFrame) {
        window.addEventListener('scroll', function() {
            const phonePosition = phoneFrame.getBoundingClientRect().top;
            const screenPosition = window.innerHeight / 1.2;
            
            if (phonePosition < screenPosition) {
                phoneFrame.style.transform = 'rotate(0deg) perspective(800px) rotateY(0deg)';
                phoneFrame.style.opacity = '1';
            }
        });
    }
    
    // Animación para el slider de potencia
    const sliderHandle = document.querySelector('.slider-handle');
    const sliderFill = document.querySelector('.slider-fill');
    const sliderValue = document.querySelector('.slider-value');
    
    if (sliderHandle && sliderFill && sliderValue) {
        let isDragging = false;
        
        sliderHandle.addEventListener('mousedown', function(e) {
            isDragging = true;
            e.preventDefault();
        });
        
        document.addEventListener('mouseup', function() {
            isDragging = false;
        });
        
        document.addEventListener('mousemove', function(e) {
            if (isDragging) {
                const sliderTrack = document.querySelector('.slider-track');
                const trackRect = sliderTrack.getBoundingClientRect();
                let position = (e.clientX - trackRect.left) / trackRect.width;
                
                // Limitar entre 0 y 1
                position = Math.max(0, Math.min(1, position));
                
                // Actualizar posición del handle y fill
                sliderHandle.style.left = position * 100 + '%';
                sliderFill.style.width = position * 100 + '%';
                
                // Actualizar valor
                const value = Math.round(position * 100);
                sliderValue.textContent = value + '%';
            }
        });
    }
    
    // Animación para los modos
    const modes = document.querySelectorAll('.mode');
    if (modes.length > 0) {
        modes.forEach(mode => {
            mode.addEventListener('click', function() {
                // Quitar clase activa de todos los modos
                modes.forEach(m => m.classList.remove('active'));
                
                // Añadir clase activa al modo clickeado
                this.classList.add('active');
            });
        });
    }
    
    // Crear partículas para la sección de la app
    const appParticles = document.getElementById('appParticles');
    if (appParticles) {
        // Crear partículas
        for (let i = 0; i < 50; i++) {
            const particle = document.createElement('div');
            particle.className = 'particle';
            particle.style.position = 'absolute';
            particle.style.width = Math.random() * 5 + 'px';
            particle.style.height = particle.style.width;
            particle.style.backgroundColor = 'rgba(255, 255, 255, ' + (Math.random() * 0.3) + ')';
            particle.style.borderRadius = '50%';
            particle.style.top = Math.random() * 100 + '%';
            particle.style.left = Math.random() * 100 + '%';
            particle.style.animation = 'float ' + (Math.random() * 10 + 5) + 's linear infinite';
            particle.style.opacity = Math.random() * 0.5;
            
            appParticles.appendChild(particle);
        }
    }
}
document.head.appendChild(style);